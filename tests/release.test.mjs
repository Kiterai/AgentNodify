import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { nextVersion, parseVersion, planRelease, runRelease } from '../scripts/release.mjs';

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
  writeFileSync(join(directory, 'CMakeLists.txt'), 'set(AGENTNODIFY_VERSION "0.1.0" CACHE STRING "Version")');
  return { GITHUB_TOKEN: 'test-token-not-a-secret', GITHUB_REPOSITORY: 'owner/repo', GITHUB_SHA: sha,
    GITHUB_OUTPUT: join(directory, 'outputs'), INSTALLER_DIRECTORY: directory, RELEASE_VERSION: '0.1.1' };
}

function runFixture(env, command, request) {
  return runRelease({ env, command, request, cmakeFile: join(env.INSTALLER_DIRECTORY, 'CMakeLists.txt') });
}

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

test('publish uploads installer and checksum before making the draft public', async t => {
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
      return Response.json({ ...body, id: 1, upload_url: 'https://uploads.github.com/repos/owner/repo/releases/1/assets{?name}' });
    }
    if (options.method === 'PATCH') assert.equal(JSON.parse(options.body).draft, false);
    return Response.json({});
  };
  await runFixture(env, 'publish', request);
  assert.deepEqual(writes, ['POST /repos/owner/repo/releases',
    'POST /repos/owner/repo/releases/1/assets', 'POST /repos/owner/repo/releases/1/assets',
    'PATCH /repos/owner/repo/releases/1']);
  assert.match(readFileSync(join(env.INSTALLER_DIRECTORY, 'SHA256SUMS.txt'), 'utf8'), /^[a-f0-9]{64}  AgentNodify-0\.1\.1-Windows-AMD64-Setup\.exe\n$/);
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
