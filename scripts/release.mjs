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

export [object Function]

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
  if (!['publish', 'verify'].includes(command)) throw new Error('Use prepare, verify or publish');
  if (plan.published) { console.log('Already published; no assets changed'); return; }
  if (plan.version !== env.RELEASE_VERSION) throw new Error('Version changed since preparation; refusing to publish');
  if (env.LEGAL_APPROVED !== 'true') throw new Error('Explicit distribution review approval is required');
  const directory = env.INSTALLER_DIRECTORY;
  if (!directory) throw new Error('INSTALLER_DIRECTORY is required');
  const installers = readdirSync(directory).filter(name => name.startsWith(`AgentNodify-${plan.version}-`) && name.endsWith('-Setup.exe'));
  if (installers.length !== 1) throw new Error('Expected exactly one versioned installer');
  const installer = join(directory, installers[0]);
  const installerDigest = createHash('sha256').update(readFileSync(installer)).digest('hex');
  const validationFile = join(directory, 'installer-validation.json');
  const validation = JSON.parse(readFileSync(validationFile, 'utf8'));
  if (validation.schema !== 1 || validation.passed !== true || validation.installerSha256 !== installerDigest
      || !Array.isArray(validation.checks) || validation.checks.length === 0) {
    throw new Error('Installer roundtrip validation is missing, failed or stale');
  }
  const signatureNames = ['installer', 'application', 'maintenance'];
  if (!Array.isArray(validation.signatures) || validation.signatures.length !== signatureNames.length
      || !signatureNames.every(name => validation.signatures.filter(signature => signature.name === name).length === 1)
      || validation.signatures.some(signature => !['Valid', 'NotSigned'].includes(signature.status))) {
    throw new Error('Authenticode validation records are missing or contain signature errors');
  }
  const signatures = signatureNames.map(name => validation.signatures?.find(signature => signature.name === name));
  if (signatures.some(signature => signature.status === 'Valid' && signature.selfSigned !== false)) {
    throw new Error('Self-signed or unclassified signing identities are test-only and cannot be published');
  }
  const allSigned = signatures.every(signature => signature?.status === 'Valid');
  if (((env.REQUIRE_SIGNED === 'true' || env.EXPECTED_SIGNER_THUMBPRINT) && !allSigned)
      || (env.EXPECTED_SIGNER_THUMBPRINT && signatures.some(signature =>
        signature?.thumbprint?.toLowerCase() !== env.EXPECTED_SIGNER_THUMBPRINT.toLowerCase()))) {
    throw new Error('Required Authenticode signature validation is incomplete');
  }
  const signatureSummary = allSigned ? 'Authenticode signatures verified for installer, application and Maintenance Tool'
    : signatures.every(signature => signature?.status === 'NotSigned') ? 'unsigned' : 'not fully signature-verified';
  const sourceName = `AgentNodify-${plan.version}-Sources.zip`;
  const sources = join(directory, sourceName);
  const recordFile = join(directory, 'distribution-release.json');
  const record = JSON.parse(readFileSync(recordFile, 'utf8'));
  const sourceDigest = createHash('sha256').update(readFileSync(sources)).digest('hex');
  if (record.schema !== 1 || record.version !== plan.version || record.commit !== sha
      || record.repository !== repo || record.sourceBundle !== sourceName
      || record.sourceBundleSha256 !== sourceDigest) {
    throw new Error('Source companion does not match this release');
  }
  if (!env.LEGAL_DIRECTORY || !record.noticeHashes
      || !['APPLICATION-LICENSE.txt', 'QT-NOTICES.txt', 'IFW-NOTICES.txt', 'SOURCE-OFFER.txt',
        'BUILD-INFO.json', 'SOURCE-MANIFEST.json'].every(name => Object.hasOwn(record.noticeHashes, name))) {
    throw new Error('Distribution notice manifest is incomplete');
  }
  if (JSON.stringify(readdirSync(env.LEGAL_DIRECTORY).sort()) !== JSON.stringify(Object.keys(record.noticeHashes).sort())) {
    throw new Error('Distribution notice file set changed after source preparation');
  }
  for (const [name, expected] of Object.entries(record.noticeHashes)) {
    if (!/^[A-Za-z0-9][A-Za-z0-9_.-]*$/.test(name) || !/^[a-f0-9]{64}$/.test(expected)
        || createHash('sha256').update(readFileSync(join(env.LEGAL_DIRECTORY, name))).digest('hex') !== expected) {
      throw new Error('Distribution notices changed after source preparation');
    }
  }
  if (env.RELEASE_PLATFORMS && env.RELEASE_PLATFORMS !== 'windows,macos') throw new Error('Unsupported release platform set');
  const mac = env.RELEASE_PLATFORMS === 'windows,macos'
    ? macosPayloads(env.MACOS_DIRECTORY, plan.version, sha, repo, command === 'verify' ? 'true' : env.MACOS_LEGAL_APPROVED) : [];
  const checksumFile = join(directory, 'SHA256SUMS.txt');
  const payloads = [sources, recordFile, validationFile, installer, ...mac];
  const sums = payloads.map(file => `${createHash('sha256').update(readFileSync(file)).digest('hex')}  ${basename(file)}\n`);
  writeFileSync(checksumFile, sums.join(''));
  if (command === 'verify') {
    console.log('All platform release inputs verified; no GitHub mutation or publication approval');
    console.log(`macOS publication approval configured: ${env.MACOS_LEGAL_APPROVED === 'true'}`);
    return;
  }
  let release = releases.find(release => release.tag_name === plan.tag);
  if (!release) {
    // Create the tag/release only after packaging and all tests have succeeded.
    release = await api('/releases', 'POST', {
      tag_name: plan.tag, target_commitish: sha, name: `AgentNodify ${plan.tag}`, draft: true,
      prerelease: false, body: `${plan.marker}\n\nWindows x64 installer (${signatureSummary}).\n`
        + (mac.length ? 'macOS arm64 and x86_64 PKG installers (ad-hoc application signature; PKG unsigned, not notarized).\n' : '')
        + `Commit: ${sha}\n`
        + `Corresponding source materials: ${sourceName} (application + upstream archives + notices/build records).\n`
        + 'SHA-256: see SHA256SUMS.txt.\n\n'
        + `Application Qt: ${record.qtVersion}; installer framework: ${record.ifwVersionOutput.split('\n')[0]}.\n\n`
        + '## Usage and limitations / 利用上の注意\n'
        + `AgentNodify is a ${mac.length ? 'Windows x64 and macOS arm64/x86_64' : 'Windows x64'} workflow editor for agent CLIs and script/shell tasks. `
        + (mac.length ? 'Each macOS architecture has a matching macOS Sources.zip companion on this release. Gatekeeper may block unsigned/unnotarized packages; no security bypass is provided.\n' : '')
        + 'External agent CLIs and their authentication are not bundled; service usage may incur charges. '
        + 'Only open and run trusted workflows: scripts and shell tasks execute with your user permissions. '
        + 'Automated smoke/installation tests do not cover every interactive workflow. Back up important projects.\n\n'
        + 'エージェントCLI・認証情報は同梱していません。実サービスの利用には料金が発生する場合があります。'
        + 'スクリプト・シェルノードは利用者の権限で実行されるため、信頼できるフローだけを実行してください。'
        + '全GUI操作を網羅した検証ではありません。重要なプロジェクトはバックアップしてください。',
    });
  }
  if (!release.draft || !release.body?.includes(plan.marker)) throw new Error('Refusing to modify an unrelated or published release');
  for (const file of [...payloads, checksumFile]) {
    const name = basename(file);
    // Retrying a failed draft is safe; never replace published release assets.
    for (const asset of await list(`/releases/${release.id}/assets`)) {
      if (asset.name === name) await api(`/releases/assets/${asset.id}`, 'DELETE');
    }
    const upload = new URL(release.upload_url.split('{')[0]);
    if (upload.origin !== 'https://uploads.github.com') throw new Error('Unexpected upload host');
    upload.searchParams.set('name', name);
    const data = readFileSync(file);
    const response = await request(upload, {
      method: 'POST', headers: { Authorization: `Bearer ${token}`, 'Content-Type': 'application/octet-stream' },
      body: data,
    });
    if (!response.ok) throw new Error(`Asset upload failed: ${response.status}`);
    const asset = await response.json();
    const expectedDigest = `sha256:${createHash('sha256').update(data).digest('hex')}`;
    if (asset.name !== name || asset.size !== data.length || asset.digest !== expectedDigest) {
      throw new Error(`Uploaded asset verification failed: ${name}`);
    }
  }
  await api(`/releases/${release.id}`, 'PATCH', { draft: false, make_latest: 'true' });
  console.log(`Published ${plan.tag}`);
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  runRelease().catch(error => { console.error(error.message); process.exitCode = 1; });
}
