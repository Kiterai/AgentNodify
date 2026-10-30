import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const control = readFileSync(new URL('../packaging/controlscript.qs', import.meta.url), 'utf8');
const componentScript = readFileSync(new URL('../packaging/installscript.qs', import.meta.url), 'utf8');

function fixture({ signed = false, requested = true, target = 'C:/Users/Test/AppData/Local/Programs/AgentNodify',
  uninstaller = false, status = 0, cli = true, detachedResult = true, removeData = '', answer = 2, executeResult = ['AgentNodifyAppDataRemoved', '0'] } = {}) {
  const values = { os: 'win', HomeDir: 'C:/Users/Test', TargetDir: target };
  const operations = [];
  const callbacks = [];
  const bases = [];
  const completed = [], finish = [], rejected = [], detached = [], started = [], executed = [], questions = [];
  values.RemoveAppData = removeData;
  const context = vm.createContext({
    installer: {
      isInstaller: () => !uninstaller, value: key => values[key], setValue: (key, value) => { values[key] = value; },
      fromNativeSeparators: path => path.replaceAll('\\', '/'),
      environmentVariable: key => key === 'SystemRoot' ? 'C:\\Windows' : 'D:\\LocalData',
      installationStarted: { connect: (receiver, callback) => callbacks.push(() => callback.call(receiver)) },
      uninstallationStarted: { connect: (receiver, callback) => started.push(() => callback.call(receiver)) },
      uninstallationFinished: { connect: (receiver, callback) => completed.push(() => callback.call(receiver)) },
      finishButtonClicked: { connect: (receiver, callback) => finish.push(() => callback.call(receiver)) },
      isUninstaller: () => uninstaller, isCommandLineInstance: () => cli, status,
      executeDetached: (...args) => { detached.push(args); return detachedResult; },
      execute: (...args) => { executed.push(args); return executeResult; },
      setInstallerBaseBinary: path => bases.push(path),
    },
    gui: { rejected: { connect: (receiver, callback) => rejected.push(() => callback.call(receiver)) } },
    QMessageBox: { Yes: 1, No: 2, question: (...args) => { questions.push(args); return answer; } },
    component: { createOperations: () => operations.push(['default']),
      addOperation: (...args) => operations.push(args), installationRequested: () => requested,
      updateRequested: () => false },
    ...(signed ? { signedInstallerBaseEnabled: true } : {}),
  });
  vm.runInContext(control + '\nnew Controller();', context);
  vm.runInContext(componentScript + '\nnew Component();', context);
  return { context, values, operations, callbacks, bases, completed, finish, rejected, detached, started, executed, questions };
}

test('default user install uses runtime LOCALAPPDATA without enabling signed maintenance', () => {
  const { values, callbacks, bases } = fixture();
  assert.equal(values.TargetDir, 'D:/LocalData/Programs/AgentNodify');
  assert.equal(values.UseSignedMaintenanceTool, 'false');
  callbacks.forEach(callback => callback());
  assert.deepEqual(bases, []);
});

test('successful CLI removal schedules a filename-only cleanup in the exact install directory', () => {
  const f = fixture({ uninstaller: true, target: 'D:/日本語 & %name%/AgentNodify' });
  f.completed.forEach(callback => callback());
  assert.equal(f.detached.length, 1);
  const [program, args, cwd] = f.detached[0];
  assert.equal(program, 'C:\\Windows/System32/cmd.exe');
  assert.equal(cwd, f.values.TargetDir);
  assert.equal(args.at(-1).includes(cwd), false);
  assert.match(args.at(-1), /del \/Q MaintenanceTool\.exe/);
  assert.doesNotMatch(args.at(-1), /rmdir|Remove-Item|\*|\.vbs/i);
  f.finish.forEach(callback => callback());
  assert.equal(f.detached.length, 1);
});

test('GUI cleanup waits for Finish or close after success, not the completion page itself', () => {
  for (const closeSignal of ['finish', 'rejected']) {
    const f = fixture({ uninstaller: true, cli: false });
    f[closeSignal].forEach(callback => callback());
    assert.equal(f.detached.length, 0);
    f.completed.forEach(callback => callback());
    assert.equal(f.detached.length, 0);
    f[closeSignal].forEach(callback => callback());
    assert.equal(f.detached.length, 1);
  }
});

test('failed, cancelled and non-uninstall actions never schedule deletion', () => {
  for (const options of [{ uninstaller: true, status: 1 }, { uninstaller: true, status: 3 }, {}]) {
    const f = fixture(options);
    f.completed.forEach(callback => callback());
    f.finish.forEach(callback => callback());
    f.rejected.forEach(callback => callback());
    assert.equal(f.detached.length, 0);
  }
});

test('explicit target is preserved and signed maintenance base is selected only when requested', () => {
  const { values, callbacks, bases } = fixture({ signed: true, target: 'E:/Custom App' });
  assert.equal(values.TargetDir, 'E:/Custom App');
  callbacks.forEach(callback => callback());
  assert.deepEqual(bases, ['E:/Custom App/maintenance/installerbase.exe']);
  const idle = fixture({ signed: true, requested: false });
  idle.callbacks.forEach(callback => callback());
  assert.deepEqual(idle.bases, []);
});

test('shortcuts have reversible IFW operations and removal prompts instead of killing', () => {
  const { context, operations } = fixture();
  vm.runInContext('Component.prototype.createOperations();', context);
  assert.deepEqual(operations.map(op => op[0]), ['default', 'CreateShortcut', 'CreateShortcut', 'FakeStopProcessForUpdate']);
  assert.ok(operations.some(op => op.includes('@TargetDir@/MaintenanceTool.exe')));
});

test('AppData cleanup is opt-in and runs only after successful removal', () => {
  for (const options of [{}, {removeData:'false'}, {removeData:'TRUE'}, {removeData:'true',status:1}, {removeData:'true',status:3}]) {
    const f=fixture({uninstaller:true,...options});
    f.started.forEach(fn=>fn()); f.completed.forEach(fn=>fn());
    assert.equal(f.executed.length,0);
  }
  const f=fixture({uninstaller:true,removeData:'true',target:'D:/untrusted & %text%'});
  f.started.forEach(fn=>fn()); assert.equal(f.executed.length,0);
  f.completed.forEach(fn=>fn()); assert.equal(f.executed.length,1);
  const [program,args,script]=f.executed[0];
  assert.match(program,/System32\/WindowsPowerShell\/v1.0\/powershell.exe$/);
  assert.equal(args.at(-1),'-');
  assert.match(script,/ReparsePoint/);
  assert.match(script,/ApplicationData','LocalApplicationData/);
  assert.equal(script.includes(f.values.TargetDir),false);
  // IFW substitutes text between at-signs even in stdin: PowerShell array
  // literals would silently corrupt the command before execution.
  assert.equal(script.includes('@'),false);
});

test('GUI asks with No as default, ignoring any stored cleanup value', () => {
  for(const answer of [1,2]) {
    const f=fixture({uninstaller:true,cli:false,removeData:'true',answer});
    f.started.forEach(fn=>fn()); assert.equal(f.questions.length,1);
    assert.equal(f.questions[0].at(-1),2);
    f.completed.forEach(fn=>fn()); assert.equal(f.executed.length,answer===1?1:0);
  }
});

test('cleanup launch/errors are reported without preventing maintenance self-removal', () => {
  for(const executeResult of [[],['refused reparse point','1'],['','0']]) {
    const f=fixture({uninstaller:true,removeData:'true',executeResult});
    f.started.forEach(fn=>fn());
    assert.throws(()=>f.completed.forEach(fn=>fn()),/AppData cleanup failed/);
    assert.equal(f.detached.length,1);
  }
});
