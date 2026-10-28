// Release metadata and publication. No third-party Node dependencies.
import { appendFileSync, readFileSync, readdirSync, writeFileSync } from 'node:fs';
import { basename, join } from 'node:path';
import { createHash } from 'node:crypto';
import { pathToFileURL } from 'node:url';

export function parseVersion(value) {
  const match = /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/.exec(value);
  if (!match) throw new Error(`Invalid stable semantic version: ${value}`);
  const parts = match.slice(1).map(Number);
  if (parts.some(part => !Number.isSafeInteger(part) || part > 2147483647)) {
    throw new Error('Version component exceeds the supported CMake range');
  }
  return parts;
}

export function nextVersion(baseline, tags) {
  let latest = parseVersion(baseline);
  for (const tag of tags) {
    if (!/^v(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/.test(tag)) continue;
    const parts = parseVersion(tag.slice(1));
    const differing = parts.findIndex((part, index) => part !== latest[index]);
    if (differing !== -1 && parts[differing] > latest[differing]) latest = parts;
  }
  latest[2]++;
  const result = latest.join('.');
  parseVersion(result);
  return result;
}

export function planRelease(baseline, tags, releases, sha) {
  if (!/^[a-f0-9]{40}$/.test(sha)) throw new Error('Expected an immutable commit SHA');
  const marker = `<!-- agentnodify-commit:${sha} -->`;
  const matches = releases.filter(release => release.body?.includes(marker));
  if (matches.length > 1) throw new Error('Multiple releases claim this commit');
  const existing = matches[0];
  if (existing) {
    const version = existing.tag_name.slice(1);
    parseVersion(version);
    if (existing.tag_name !== `v${version}` || existing.prerelease) throw new Error('Unexpected release type');
    const tag = tags.find(tag => tag.name === existing.tag_name);
    if ((tag && tag.commit.sha !== sha) || (!tag && !existing.draft)) {
      throw new Error('Release tag does not point to the expected commit');
    }
    if (existing.target_commitish !== sha) throw new Error('Release target is not the expected commit');
    return { version, tag: existing.tag_name, published: !existing.draft, marker };
  }
  const version = nextVersion(baseline, [...tags.map(tag => tag.name), ...releases.map(release => release.tag_name)]);
  return { version, tag: `v${version}`, published: false, marker };
}

export async function runRelease({ env = process.env, command = process.argv[2], request = fetch,
  cmakeFile = 'CMakeLists.txt' } = {}) {
  const { GITHUB_TOKEN: token, GITHUB_REPOSITORY: repo, GITHUB_SHA: sha } = env;
  if (!token || !/^[\w.-]+\/[\w.-]+$/.test(repo ?? '')) throw new Error('Missing release credentials/repository');
  const apiRoot = `https://api.github.com/repos/${repo}`;
  async function api(path, method = 'GET', data) {
    const response = await request(`${apiRoot}${path}`, {
      method,
      headers: { Authorization: `Bearer ${token}`, Accept: 'application/vnd.github+json',
        'X-GitHub-Api-Version': '2022-11-28', ...(data ? { 'Content-Type': 'application/json' } : {}) },
      body: data ? JSON.stringify(data) : undefined,
    });
    if (!response.ok) throw new Error(`GitHub ${method} ${path}: ${response.status}`);
    return response.status === 204 ? null : response.json();
  }
  async function list(path) {
    const result = [];
    for (let page = 1; ; page++) {
      const batch = await api(`${path}?per_page=100&page=${page}`);
      result.push(...batch);
      if (batch.length < 100) return result;
    }
  }
  const cmake = readFileSync(cmakeFile, 'utf8');
  const baseline = /set\(AGENTNODIFY_VERSION "([^"]+)" CACHE STRING/.exec(cmake)?.[1];
  if (!baseline) throw new Error('CMake baseline version not found');
  const [tags, releases] = await Promise.all([list('/tags'), list('/releases')]);
  const plan = planRelease(baseline, tags, releases, sha);
  if (command === 'prepare') {
    if (!env.GITHUB_OUTPUT) throw new Error('GITHUB_OUTPUT is required');
    appendFileSync(env.GITHUB_OUTPUT,
      `version=${plan.version}\ntag=${plan.tag}\npublished=${plan.published}\n`);
    console.log(`${plan.tag}: ${plan.published ? 'already published; skipping' : 'ready to package'}`);
    return;
  }
  if (command !== 'publish') throw new Error('Use prepare or publish');
  if (plan.published) { console.log('Already published; no assets changed'); return; }
  if (plan.version !== env.RELEASE_VERSION) throw new Error('Version changed since preparation; refusing to publish');
  const directory = env.INSTALLER_DIRECTORY;
  if (!directory) throw new Error('INSTALLER_DIRECTORY is required');
  const installers = readdirSync(directory).filter(name => name.startsWith(`AgentNodify-${plan.version}-`) && name.endsWith('-Setup.exe'));
  if (installers.length !== 1) throw new Error('Expected exactly one versioned installer');
  const installer = join(directory, installers[0]);
  const checksumFile = join(directory, 'SHA256SUMS.txt');
  const digest = createHash('sha256').update(readFileSync(installer)).digest('hex');
  writeFileSync(checksumFile, `${digest}  ${basename(installer)}\n`);
  let release = releases.find(release => release.tag_name === plan.tag);
  if (!release) {
    // Create the tag/release only after packaging and all tests have succeeded.
    release = await api('/releases', 'POST', {
      tag_name: plan.tag, target_commitish: sha, name: `AgentNodify ${plan.tag}`, draft: true,
      prerelease: false, body: `${plan.marker}\n\nWindows x64 installer (unsigned).\nCommit: ${sha}\nSHA-256: see SHA256SUMS.txt.`,
    });
  }
  if (!release.draft || !release.body?.includes(plan.marker)) throw new Error('Refusing to modify an unrelated or published release');
  for (const file of [installer, checksumFile]) {
    const name = basename(file);
    // Retrying a failed draft is safe; never replace published release assets.
    for (const asset of await list(`/releases/${release.id}/assets`)) {
      if (asset.name === name) await api(`/releases/assets/${asset.id}`, 'DELETE');
    }
    const upload = new URL(release.upload_url.split('{')[0]);
    if (upload.origin !== 'https://uploads.github.com') throw new Error('Unexpected upload host');
    upload.searchParams.set('name', name);
    const response = await request(upload, {
      method: 'POST', headers: { Authorization: `Bearer ${token}`, 'Content-Type': 'application/octet-stream' },
      body: readFileSync(file),
    });
    if (!response.ok) throw new Error(`Asset upload failed: ${response.status}`);
  }
  await api(`/releases/${release.id}`, 'PATCH', { draft: false, make_latest: 'true' });
  console.log(`Published ${plan.tag}`);
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  runRelease().catch(error => { console.error(error.message); process.exitCode = 1; });
}
