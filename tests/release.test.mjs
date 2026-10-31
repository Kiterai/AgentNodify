import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { createHash } from 'node:crypto';
import { nextVersion, parseVersion, planRelease, runRelease, macosPayloads } from '../scripts/release.mjs';

const sha = 'a'.repeat(40);
const marker = `<!-- agentnodify-commit:${sha} -->`;
const release = { tag_name: 'v0.1.1', target_commitish: sha, body: marker, draft: true, prerelease: false };

test('first release increments the CMake baseline', () => {
  assert.equal(nextVersion('0.1.0', []), '0.1.1');
});
test('stable tags are compared numerically, not lexically', () => {
  assert.equal(nextVersion('0.1.0', ['v0.1.9', 'v0.1.10', 'v0.1.2']), '0.1.11');
  assert.equal(nextVersion('0.1.0', ['v1.9.99', 'v1.10.0']), '1.10.1');
});
test('prereleases, malformed tags and unprefixed versions are ignored', () => {
  assert.equal(nextVersion('0.1.0', ['v9.0.0-rc.1', '2.0.0', 'v01.2.3', 'other']), '0.1.1');
});
test('raising the baseline starts a new major/minor line', () => {
  assert.equal(nextVersion('1.0.0', ['v0.9.99']), '1.0.1');
});
test('invalid or overflowing versions fail', () => {
  for (const value of ['01.2.3', '1.2', '-1.0.0', '1.2.3+meta', '2147483648.0.0']) {
    assert.throws(() => parseVersion(value));
  }
  assert.throws(() => nextVersion('0.1.2147483647', []));
});
test('draft retries retain their version', () => {
  assert.deepEqual(planRelease('0.1.0', [], [release], sha), {
    version: '0.1.1', tag: 'v0.1.1', published: false, marker,
  });
});
test('published retries skip without consuming a version', () => {
  const tags = [{ name: 'v0.1.1', commit: { sha } }];
  assert.equal(planRelease('0.1.0', tags, [{ ...release, draft: false }], sha).published, true);
});
test('drafts also reserve versions for later commits', () => {
  assert.equal(planRelease('0.1.0', [], [release], 'b'.repeat(40)).version, '0.1.2');
});
test('tag tampering and duplicate release markers fail closed', () => {
  assert.throws(() => planRelease('0.1.0', [{ name: 'v0.1.1', commit: { sha: 'b'.repeat(40) } }], [release], sha));
  assert.throws(() => planRelease('0.1.0', [], [release, release], sha));
  assert.throws(() => planRelease('0.1.0', [], [{ ...release, draft: false }], sha));
  assert.throws(() => planRelease('0.1.0', [], [{ ...release, target_commitish: 'master' }], sha));
});

function fixture(t) {
  const directory = mkdtempSync(join(process.cwd(), 'build-release-test-'));
  t.after(() => rmSync(directory, { recursive: true, force: true }));
  writeFileSync(join(directory, 'AgentNodify-0.1.1-Windows-AMD64-Setup.exe'), 'test installer');
  writeFileSync(join(directory, 'installer-validation.json'), JSON.stringify({
    schema: 1, passed: true, installerSha256: createHash('sha256').update('test installer').digest('hex'),
    checks: ['fixture validation - not real installation evidence'],
    signatures: ['installer', 'application', 'maintenance'].map(name => ({ name, status: 'NotSigned', thumbprint: null })),
  }));
  const sourceName = 'AgentNodify-0.1.1-Sources.zip';
  writeFileSync(join(directory, sourceName), 'test source bundle');
  const legalDirectory = join(directory, 'legal');
  mkdirSync(legalDirectory);
  const noticeHashes = {};
  for (const name of ['APPLICATION-LICENSE.txt', 'QT-NOTICES.txt', 'IFW-NOTICES.txt',
    'SOURCE-OFFER.txt', 'BUILD-INFO.json', 'SOURCE-MANIFEST.json']) {
    writeFileSync(join(legalDirectory, name), 'fixture notice - not distribution material');
    noticeHashes[name] = createHash('sha256').update('fixture notice - not distribution material').digest('hex');
  }
  writeFileSync(join(directory, 'distribution-release.json'), JSON.stringify({
    schema: 1, version: '0.1.1', commit: sha, repository: 'owner/repo', sourceBundle: sourceName,
    sourceBundleSha256: createHash('sha256').update('test source bundle').digest('hex'),
    qtVersion: '6.12.0', ifwVersionOutput: 'IFW Version: 4.7.0, built with Qt 6.6.0.',
    noticeHashes,
  }));
  writeFileSync(join(directory, 'CMakeLists.txt'), 'set(AGENTNODIFY_VERSION "0.1.0" CACHE STRING "Version")');
  return { GITHUB_TOKEN: 'test-token-not-a-secret', GITHUB_REPOSITORY: 'owner/repo', GITHUB_SHA: sha,
    GITHUB_OUTPUT: join(directory, 'outputs'), INSTALLER_DIRECTORY: directory, LEGAL_DIRECTORY: legalDirectory,
    RELEASE_VERSION: '0.1.1', LEGAL_APPROVED: 'true' };
}

function runFixture(env, command, request) {
  return runRelease({ env, command, request, cmakeFile: join(env.INSTALLER_DIRECTORY, 'CMakeLists.txt') });
}

function macFixture(t) {
  const env = fixture(t);
  env.RELEASE_PLATFORMS = 'windows,macos';
  env.MACOS_LEGAL_APPROVED = 'true';
  env.MACOS_DIRECTORY = join(env.INSTALLER_DIRECTORY, 'macos');
  const digest = value => createHash('sha256').update(value).digest('hex');
  for (const architecture of ['arm64', 'x86_64']) {
    const dir = join(env.MACOS_DIRECTORY, architecture, 'installer');
    const legal = join(env.MACOS_DIRECTORY, architecture, 'distribution');
    mkdirSync(dir, { recursive: true }); mkdirSync(legal);
    const installer = `AgentNodify-0.1.1-macOS-${architecture}-Setup.pkg`;
    writeFileSync(join(dir, installer), 'mac fixture');
    writeFileSync(join(dir, `macos-${architecture}-installer-validation.json`), JSON.stringify({
      schema: 1, success: true, installed: true, architecture, version: '0.1.1', commit: sha,
      installer, sha256: digest('mac fixture'), checks: ['payload', 'receipt', 'cocoa', 'signature', 'uninstall', 'receipt_removed'],
    }));
    writeFileSync(join(dir, `macos-${architecture}-package-validation.json`), JSON.stringify({
      schema: 1, architecture, version: '0.1.1', installer, sha256: digest('mac fixture'),
      applicationSignature: 'ad-hoc', installerSigned: false, notarized: false,
    }));
    const sourceBundle = `AgentNodify-0.1.1-macOS-${architecture}-Sources.zip`;
    writeFileSync(join(dir, sourceBundle), 'mac source fixture');
    const noticeHashes = {};
    for (const name of ['APPLICATION-LICENSE.txt', 'QT-NOTICES.txt', 'SOURCE-OFFER.txt', 'BUILD-INFO.json', 'SOURCE-MANIFEST.json']) {
      writeFileSync(join(legal, name), 'mac notice fixture'); noticeHashes[name] = digest('mac notice fixture');
    }
    writeFileSync(join(dir, `macos-${architecture}-distribution-release.json`), JSON.stringify({
      schema: 1, architecture, version: '0.1.1', commit: sha, repository: 'owner/repo',
      profile: 'macos-official-qt-6.12.0', qtVersion: '6.12.0', sourceBundle,
      sourceBundleSha256: digest('mac source fixture'), noticeHashes,
    }));
  }
  return env;
}

test('multiplatform release requires independent macOS approval and both validated architectures', async t => {
  const env = macFixture(t);
  const request = async (url, options) => { assert.equal(options.method, 'GET'); return Response.json([]); };
  await assert.rejects(runFixture({ ...env, MACOS_LEGAL_APPROVED: undefined }, 'publish', request), /macOS distribution review/);
  assert.equal(macosPayloads(env.MACOS_DIRECTORY, '0.1.1', sha, 'owner/repo', 'true').length, 10);
  const report = join(env.MACOS_DIRECTORY, 'x86_64/installer/macos-x86_64-installer-validation.json');
  const original = JSON.parse(readFileSync(report, 'utf8'));
  for (const change of [{ commit: 'b'.repeat(40) }, { installed: false }, { sha256: '0'.repeat(64) }, { checks: [] }]) {
    writeFileSync(report, JSON.stringify({ ...original, ...change }));
    await assert.rejects(runFixture(env, 'publish', request), /roundtrip evidence/);
  }
});

test('macOS source changes and missing CPU block publication before API writes', async t => {
  const env = macFixture(t);
  const request = async (url, options) => { assert.equal(options.method, 'GET'); return Response.json([]); };
  const notice = join(env.MACOS_DIRECTORY, 'arm64/distribution/SOURCE-OFFER.txt');
  writeFileSync(notice, 'changed');
  await assert.rejects(runFixture(env, 'publish', request), /notice changed/);
  writeFileSync(notice, 'mac notice fixture');
  rmSync(join(env.MACOS_DIRECTORY, 'x86_64/installer/AgentNodify-0.1.1-macOS-x86_64-Setup.pkg'));
  await assert.rejects(runFixture(env, 'publish', request), /ENOENT/);
});

test('combined verification performs no GitHub writes and is not publication approval', async t => {
  const env = macFixture(t); delete env.MACOS_LEGAL_APPROVED;
  const request = async (url, options) => { assert.equal(options.method, 'GET'); return Response.json([]); };
  await runFixture(env, 'verify', request);
  assert.match(readFileSync(join(env.INSTALLER_DIRECTORY, 'SHA256SUMS.txt'), 'utf8'), /macOS-arm64-Setup.pkg/);
  await assert.rejects(runFixture(env, 'publish', request), /macOS distribution review/);
});

test('all platform payloads are uploaded before the one draft becomes public', async t => {
  const env = macFixture(t); const uploads = []; let published = false;
  const request = async (url, options) => {
    if (options.method === 'GET') return Response.json([]);
    if (new URL(url).host === 'uploads.github.com') {
      const name = new URL(url).searchParams.get('name'); uploads.push(name);
      return Response.json({ name, size: options.body.length, digest: `sha256:${createHash('sha256').update(options.body).digest('hex')}` });
    }
    if (options.method === 'POST') {
      const body = JSON.parse(options.body); assert.match(body.body, /macOS arm64 and x86_64/);
      return Response.json({ ...body, id: 1, upload_url: 'https://uploads.github.com/repos/owner/repo/releases/1/assets{?name}' });
    }
    if (options.method === 'PATCH') { assert.equal(uploads.length, 15); published = true; }
    return Response.json({});
  };
  await runFixture(env, 'publish', request); assert.equal(published, true);
  for (const arch of ['arm64', 'x86_64']) assert(uploads.includes(`AgentNodify-0.1.1-macOS-${arch}-Setup.pkg`));
  assert.equal(new Set(uploads).size, 15);
});

test('prepare only reads API state and emits validated outputs', async t => {
  const env = fixture(t);
  const request = async (url, options) => {
    assert.equal(options.method, 'GET');
    assert.match(String(url), /\/(tags|releases)\?per_page=100&page=1$/);
    return Response.json([]);
  };
  await runFixture(env, 'prepare', request);
  assert.equal(readFileSync(env.GITHUB_OUTPUT, 'utf8'), 'version=0.1.1\ntag=v0.1.1\npublished=false\n');
});

test('publish verifies sources, record, validation, installer and checksum before making the draft public', async t => {
  const env = fixture(t);
  const writes = [];
  const request = async (url, options) => {
    const path = new URL(url).pathname;
    if (options.method === 'GET') return Response.json([]);
    writes.push(`${options.method} ${path}`);
    if (path === '/repos/owner/repo/releases' && options.method === 'POST') {
      const body = JSON.parse(options.body);
      assert.equal(body.target_commitish, sha);
      assert.equal(body.draft, true);
      assert.equal(body.tag_name, 'v0.1.1');
      assert.doesNotMatch(body.body, /Unsigned release|未署名リリース|SmartScreen|test certificate/);
      assert.match(body.body, /Windows x64 installer \(unsigned\)/);
      assert.match(body.body, /Only open and run trusted workflows/);
      assert.match(body.body, /Application Qt: 6\.12\.0/);
      return Response.json({ ...body, id: 1, upload_url: 'https://uploads.github.com/repos/owner/repo/releases/1/assets{?name}' });
    }
    if (options.method === 'PATCH') assert.equal(JSON.parse(options.body).draft, false);
    if (new URL(url).host === 'uploads.github.com') {
      return Response.json({ name: new URL(url).searchParams.get('name'), size: options.body.length,
        digest: `sha256:${createHash('sha256').update(options.body).digest('hex')}` });
    }
    return Response.json({});
  };
  await runFixture(env, 'publish', request);
  assert.deepEqual(writes, ['POST /repos/owner/repo/releases',
    'POST /repos/owner/repo/releases/1/assets', 'POST /repos/owner/repo/releases/1/assets',
    'POST /repos/owner/repo/releases/1/assets', 'POST /repos/owner/repo/releases/1/assets',
    'POST /repos/owner/repo/releases/1/assets',
    'PATCH /repos/owner/repo/releases/1']);
  const checksums = readFileSync(join(env.INSTALLER_DIRECTORY, 'SHA256SUMS.txt'), 'utf8');
  assert.match(checksums, /^[a-f0-9]{64}  AgentNodify-0\.1\.1-Sources\.zip\n/);
  assert.match(checksums, /[a-f0-9]{64}  AgentNodify-0\.1\.1-Windows-AMD64-Setup\.exe\n$/);
});

test('failed or stale installer roundtrip evidence cannot create a release', async t => {
  const env = fixture(t);
  const request = async (url, options) => {
    assert.equal(options.method, 'GET');
    return Response.json([]);
  };
  const path = join(env.INSTALLER_DIRECTORY, 'installer-validation.json');
  const original = JSON.parse(readFileSync(path, 'utf8'));
  writeFileSync(path, JSON.stringify({ ...original, passed: false }));
  await assert.rejects(runFixture(env, 'publish', request), /roundtrip validation/);
  writeFileSync(path, JSON.stringify({ ...original, installerSha256: '0'.repeat(64) }));
  await assert.rejects(runFixture(env, 'publish', request), /roundtrip validation/);
});

test('signed-only policy rejects an unsigned installer report', async t => {
  const env = fixture(t);
  const request = async (url, options) => {
    assert.equal(options.method, 'GET');
    return Response.json([]);
  };
  await assert.rejects(runFixture({ ...env, REQUIRE_SIGNED: 'true' }, 'publish', request), /Authenticode/);
  await assert.rejects(runFixture({ ...env, EXPECTED_SIGNER_THUMBPRINT: 'a'.repeat(40) }, 'publish', request), /Authenticode/);
  const file = join(env.INSTALLER_DIRECTORY, 'installer-validation.json');
  const report = JSON.parse(readFileSync(file, 'utf8'));
  report.signatures[0].status = 'HashMismatch';
  writeFileSync(file, JSON.stringify(report));
  await assert.rejects(runFixture(env, 'publish', request), /Authenticode/);
});

test('self-signed and unclassified valid identities cannot be published', async t => {
  const env = fixture(t);
  const request = async (url, options) => {
    assert.equal(options.method, 'GET');
    return Response.json([]);
  };
  const file = join(env.INSTALLER_DIRECTORY, 'installer-validation.json');
  const report = JSON.parse(readFileSync(file, 'utf8'));
  for (const selfSigned of [true, undefined]) {
    report.signatures[0] = { name: 'installer', status: 'Valid', thumbprint: 'a'.repeat(40), selfSigned };
    writeFileSync(file, JSON.stringify(report));
    await assert.rejects(runFixture(env, 'publish', request), /test-only/);
  }
});

test('unapproved or changed source material cannot create a release', async t => {
  const env = fixture(t);
  const request = async (url, options) => {
    assert.equal(options.method, 'GET');
    return Response.json([]);
  };
  await assert.rejects(runFixture({ ...env, LEGAL_APPROVED: undefined }, 'publish', request), /approval/);
  writeFileSync(join(env.INSTALLER_DIRECTORY, 'AgentNodify-0.1.1-Sources.zip'), 'changed source');
  await assert.rejects(runFixture(env, 'publish', request), /Source companion/);
});

test('missing source companion cannot create a release', async t => {
  const env = fixture(t);
  rmSync(join(env.INSTALLER_DIRECTORY, 'AgentNodify-0.1.1-Sources.zip'));
  const request = async (url, options) => {
    assert.equal(options.method, 'GET');
    return Response.json([]);
  };
  await assert.rejects(runFixture(env, 'publish', request), /ENOENT/);
});

test('changed notice material cannot create a release', async t => {
  const env = fixture(t);
  writeFileSync(join(env.LEGAL_DIRECTORY, 'SOURCE-OFFER.txt'), 'changed offer');
  const request = async (url, options) => {
    assert.equal(options.method, 'GET');
    return Response.json([]);
  };
  await assert.rejects(runFixture(env, 'publish', request), /notices changed/);
});

test('server-reported checksum mismatch leaves release unpublished', async t => {
  const env = fixture(t);
  let published = false;
  const request = async (url, options) => {
    if (options.method === 'PATCH') published = true;
    if (options.method === 'GET') return Response.json([]);
    if (new URL(url).host === 'uploads.github.com') {
      return Response.json({ name: new URL(url).searchParams.get('name'), size: options.body.length,
        digest: `sha256:${'0'.repeat(64)}` });
    }
    return Response.json({ ...release, id: 1, upload_url: 'https://uploads.github.com/assets{?name}' });
  };
  await assert.rejects(runFixture(env, 'publish', request), /verification failed/);
  assert.equal(published, false);
});

test('failed upload leaves the release unpublished', async t => {
  const env = fixture(t);
  let published = false;
  const request = async (url, options) => {
    if (options.method === 'PATCH') published = true;
    if (options.method === 'GET') return Response.json([]);
    if (new URL(url).host === 'uploads.github.com') return new Response('', { status: 500 });
    return Response.json({ ...release, id: 1, upload_url: 'https://uploads.github.com/assets{?name}' });
  };
  await assert.rejects(runFixture(env, 'publish', request), /Asset upload failed/);
  assert.equal(published, false);
});

test('version races and missing installers create no release', async t => {
  const env = fixture(t);
  const request = async (url, options) => {
    assert.equal(options.method, 'GET');
    return Response.json([]);
  };
  await assert.rejects(runFixture({ ...env, RELEASE_VERSION: '0.1.2' }, 'publish', request), /Version changed/);
  rmSync(join(env.INSTALLER_DIRECTORY, 'AgentNodify-0.1.1-Windows-AMD64-Setup.exe'));
  await assert.rejects(runFixture(env, 'publish', request), /exactly one/);
});
