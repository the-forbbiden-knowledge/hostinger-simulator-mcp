# Local-only operation. No remote push is performed.
$ErrorActionPreference = 'Stop'
Set-Location (Resolve-Path (Join-Path $PSScriptRoot '../..'))
$commits = @(git rev-list --count HEAD)
if ($LASTEXITCODE -ne 0 -or $commits[0].Trim() -ne '1') { throw 'Expected exactly one existing commit.' }
$message = git log -1 --format=%s
if ($message -ne 'Initial commit') { throw 'Unexpected commit message.' }
git add -A
if ($LASTEXITCODE -ne 0) { throw 'Git add failed.' }
git diff --cached --quiet
if ($LASTEXITCODE -eq 0) { Write-Host 'No staged changes. Repository already clean.'; exit 0 }
git commit --amend --no-edit
if ($LASTEXITCODE -ne 0) { throw 'Commit amendment failed.' }
$count = (git rev-list --count HEAD).Trim()
if ($count -ne '1') { throw 'Single-commit invariant violated.' }
if ((git status --porcelain).Length -ne 0) { throw 'Working tree is not clean.' }
Write-Host 'Exactly one local root commit (Initial commit). Worktree clean. Nothing pushed.'
