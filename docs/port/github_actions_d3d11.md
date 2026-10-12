# GitHub Actions Workflow: DirectX 11 Build

## Overview

This GitHub Actions workflow automatically builds the Windows version of Tenkaichi3Decomp with DirectX 11 support and optionally creates a pre-release.

## Workflow File

`.github/workflows/build-d3d11.yml`

## Triggers

### Automatic Triggers:
1. **Push to branches**: `main`, `d3d11-integration`
   - Only when D3D11 files are modified
2. **Pull requests** to `main` branch
3. **Manual dispatch** via GitHub UI (with optional pre-release creation)

### Manual Trigger:
Go to: `Actions` → `Build Windows with DirectX 11` → `Run workflow`
- Choose branch
- Check "Create pre-release" to publish a pre-release

## Workflow Steps

### 1. Environment Setup
- Checkout repository with submodules
- Setup Python 3.11
- Setup MSVC compiler
- Install Ninja build system

### 2. Disc Image Handling
- **Option A**: Use cached disc data (if available)
- **Option B**: Download from `DISC_IMAGE_URL` secret (if configured)
- **Option C**: Mock build without disc (limited functionality)

### 3. Build Process
- Run `configure.py` with disc image
- Build with Ninja
- Verify executable creation

### 4. Packaging
- Copy executable and setup files
- Generate build information
- Create comprehensive changelog

### 5. Artifact Upload
- Upload build artifacts (30-day retention)
- Artifacts include: executable, setup, README, build info

### 6. Pre-release Creation (Optional)
- Creates GitHub pre-release with tag `d3d11-experimental-{run_number}`
- Includes full changelog and build artifacts
- Marked as experimental/pre-release

### 7. PR Comments
- Automatically comments on PRs with build status and changelog

## Configuration

### Required Secrets (Optional):
- `DISC_IMAGE_URL`: URL to download the PS2 disc image
  - Only needed if you want automated builds without manual disc provision
  - Can be a private file server, cloud storage with signed URL, etc.

### Environment Variables:
- `BT3_GPU_API=d3d11`: Force DirectX 11 backend at runtime

## Usage

### For Contributors:
1. Push changes to `d3d11-integration` branch
2. Workflow runs automatically
3. Download artifacts from workflow run
4. Test on your hardware
5. Report results in PR

### For Maintainers:
1. Review PR with D3D11 changes
2. Check workflow build status
3. Download and test artifacts
4. Manually trigger with "Create pre-release" for public testing

### For Testers:
1. Go to Actions tab
2. Find latest successful workflow run
3. Download `Tenkaichi3Decomp-Windows-D3D11-{sha}` artifact
4. Extract and test
5. Report results in issues

## Changelog Format

The workflow generates a comprehensive changelog including:
- What's new
- Technical details
- Status and limitations
- Testing instructions
- Requirements
- Build information
- Feedback request

## Pre-release Tags

Format: `d3d11-experimental-{run_number}`

Example: `d3d11-experimental-42`

## Artifacts

### Build Artifact Contents:
- `Tenkaichi3Decomp.exe` - Main executable
- `Tenkaichi3Decomp-setup.exe` - Setup utility
- `README.md` - Project README
- `D3D11_README.md` - DirectX 11 integration docs
- `BUILD_INFO.txt` - Build metadata

### Retention:
- Artifacts: 30 days
- Pre-releases: Permanent (until manually deleted)

## Testing Workflow Locally

### Prerequisites:
- Windows 10/11
- Visual Studio 2019+
- Python 3.11+
- Ninja (`pip install ninja`)
- PS2 disc image

### Steps:
```cmd
git clone https://github.com/Maherilaza/Tenkaichi3Decomp.git
cd Tenkaichi3Decomp
python configure.py --iso "path\to\disc.iso"
ninja
```

## Troubleshooting

### Build Fails: "Disc image not found"
- Add `DISC_IMAGE_URL` secret, or
- Use cached disc data from previous runs, or
- Accept limited mock build

### Build Fails: "MSVC not found"
- Ensure `microsoft/setup-msbuild@v2` action succeeds
- Check Windows runner has MSVC installed

### Artifact Upload Fails
- Check artifact size (GitHub limit: 2GB per artifact)
- Verify paths exist before upload

### Pre-release Not Created
- Ensure `GITHUB_TOKEN` has write permissions
- Check if running on fork (secrets not available)
- Verify trigger conditions met

## Customization

### Change Build Configuration:
Edit `configure.py` parameters in workflow

### Add More Artifacts:
Modify "Package build artifacts" step

### Change Pre-release Naming:
Edit `tag_name` in "Create pre-release" step

### Modify Changelog:
Edit "Create changelog" step

## Security Notes

- Never commit disc images to repository
- Use secrets for sensitive URLs
- Pre-releases are public (don't include proprietary data)
- Artifacts are accessible to anyone with repository access

## Future Enhancements

- [ ] Add Linux build with Wine testing
- [ ] Automated screenshot comparison tests
- [ ] Performance benchmarking
- [ ] Multi-GPU testing matrix
- [ ] Automated PR status checks
