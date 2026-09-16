# Publishing the Python package

The **Python wheels** workflow (`.github/workflows/python-wheels.yml`) can
publish `kepler-formal` to PyPI through a manual run on `main`. Publishing is
off by default. Its separate `published_najaeda` input is also off by default.
PRs, `v*` tags, and manual builds with `publish` unchecked only produce tested
wheel artifacts; they never publish to PyPI.

This is separate from [binary releases](releasing.md). Do not create a tag or
run `tools/release.sh` for a Python-only release: `v*` tags also trigger the
standalone binary release workflow. The manual Python workflow creates no
tags or GitHub Releases and does not publish the MCP add-on.

The Python package verifies live NajaEDA netlists with `verify_designs()`.
NajaEDA owns file loading and the caller owns the netlists' lifetime; Kepler
does not delete them after verification. The Python `verify()`, `run_cli()`,
and `python -m kepler_formal` entrypoints are removed. Use NajaEDA to load
designs before calling the Python API, or the standalone binary for file/YAML
workflows.

## One-time setup

Complete these account-side settings before requesting a publication. Merely
declaring an environment in the workflow does not configure its protections.

1. In `keplertech/kepler-formal`, open **Settings → Environments** and create
   an environment named `pypi`.
2. Configure **required reviewers** for release approval. Under deployment
   branches and tags, allow only the branch `main`, with no tag rule. Restrict
   administrator bypass where available. These settings protect the publisher
   independently of the workflow's own branch checks.
3. On PyPI, configure a GitHub **Trusted Publisher**. For a new project, use
   [pending publisher registration](https://pypi.org/manage/account/publishing/).
   If the project already exists and you own it, use its **Manage → Publishing**
   page instead. Enter:

   - PyPI project: `kepler-formal`
   - GitHub owner: `keplertech`
   - Repository: `kepler-formal`
   - Workflow filename: `python-wheels.yml` (not its full path)
   - Environment: `pypi`

No stored PyPI API token is needed. Only the separate publishing job receives
`id-token: write`; build jobs do not. The publisher downloads the current
run's four wheel artifacts and uploads them without checking out or building
source in the privileged job.

See the official [PyPI setup instructions](https://docs.pypi.org/trusted-publishers/adding-a-publisher/),
[Trusted Publishing instructions](https://docs.pypi.org/trusted-publishers/using-a-publisher/),
and [GitHub environment protection documentation](https://docs.github.com/en/actions/reference/workflows-and-actions/deployments-and-environments).

## Build without publishing

After the workflow is present on the repository's default branch, open
**Actions → Python wheels → Run workflow**. Leave `publish` unchecked and
`version` empty. Choose the NajaEDA provider with `published_najaeda`:

| `published_najaeda` | Build and test provider |
| --- | --- |
| Unchecked (default) | Locally built NajaEDA `0.7.24.dev0` from the pinned submodule, using its shared-runtime SDK. |
| Checked | Published NajaEDA `0.7.24` wheels, using KF's explicitly enabled compatibility integration. |

The workflow builds, repairs, and tests wheels, then stores them as Actions
artifacts. Both modes can be used to check a branch before merging. Enabling
the published provider does **not** enable publication.

The published mode sets `KEPLER_USE_PUBLISHED_NAJAEDA=1` for the wheel helpers,
including Linux containers. Before building, `ci/prepare_python_release.py`
updates the build checkout's two NajaEDA requirements to `najaeda==0.7.24`
and enables the CMake option `KEPLER_USE_PUBLISHED_NAJAEDA`. These edits are
not committed: repository defaults remain on the development provider with
the option off. Build and runtime pins must match, so installations of a
published-provider Kepler wheel select the same NajaEDA release.

Published NajaEDA does not supply the development shared-runtime SDK. This
compatibility path is deliberately pinned to `0.7.24` and uses released native
symbols, including the private DNL cache integration. It is not a general ABI
guarantee for arbitrary NajaEDA versions; any provider upgrade requires a new
compatibility review and the complete wheel tests. It does not change NajaEDA
or bundle a second Naja runtime.

The [local source regression](python-regression.md)
remains separate and continues building both packages from the pinned source
without wheels or publishing.

## Publish a release

Publication requires both `published_najaeda` and `publish`. Development
provider builds cannot be uploaded. The published-provider build downloads
and links against NajaEDA's actual distributed wheels instead of rebuilding a
same-version provider locally.

1. Merge the intended code and workflow changes into `main` and confirm its
   checks pass. Choose an unused PyPI release version. The Python package
   version is read from `src/bin/KeplerVersion.h.in`, the same source used by
   CMake and Python package metadata;
   merge any necessary version change before starting the workflow. The
   publishing workflow does not bump or override any version declarations.
2. Open **Actions → Python wheels → Run workflow**, select `main`, check
   `published_najaeda` and `publish`, and enter that exact version in `version`.
3. All four matrix jobs validate the version and print the selected commit SHA in
   their summaries, then build, repair, and test the wheels from that run's
   checkout. A mismatched or missing version, another branch, another
   repository, or missing published-provider selection fails validation.
   Publication waits for all four jobs to succeed.
4. Review the commit, version, test results, and artifacts. Approve the waiting
   `pypi` environment deployment to allow the publishing job to run.
5. Confirm the release on [PyPI](https://pypi.org/project/kepler-formal/) and
   test installation in a fresh virtual environment on a supported platform:

   ```bash
   python -m pip install "kepler-formal==<released-version>"
   python -c "import kepler_formal; from kepler_formal import najaeda; print(kepler_formal.__version__)"
   ```

## Wheel coverage

The matrix matches the vendored NajaEDA workflow: **26 wheels** per release.

| Platform | Architecture | CPython versions | Wheels |
| --- | --- | --- | --- |
| Linux (`manylinux_2_28`) | x86_64 | 3.10–3.15, plus 3.14t | 7 |
| Linux (`manylinux_2_28`) | aarch64 | 3.10–3.15 | 6 |
| macOS | arm64 | 3.10–3.15, plus 3.14t | 7 |
| Windows | AMD64 | 3.10–3.15 | 6 |

Python 3.15 prerelease builds are explicitly enabled in cibuildwheel. The
3.14t wheels target free-threaded interpreters but require the GIL for native
verification; they do not promise concurrent, GIL-free engine calls.

`ci/check_wheel_matrix.py` compares cibuildwheel's actual selected build IDs
against NajaEDA's checked-in matrix and checks that publishing includes every
platform artifact. A Naja update that changes its matrix requires an explicit
corresponding update here. Every wheel runs the package tests and native
dependency/shared-runtime checks after repair. Windows reuses Naja's pregenerated
parser and vcpkg dependency approach, with KF-owned solver compatibility code.
The wheel smoke test loads designs through NajaEDA, verifies equivalent and
different pairs with all three solvers, and checks that the caller's designs
and selected top survive repeated verification and native errors. KF's Python
extension contains no Verilog file-loading frontend.

This workflow publishes wheels only, not a source distribution. Intel macOS,
musllinux, and Windows ARM64 wheels are not in NajaEDA's referenced matrix and
are not added here.

## Failure handling

Failed builds or missing platform artifacts prevent publication. PyPI upload
keeps metadata verification and attestations enabled, and does not silently
skip existing files. If an upload fails partway through, inspect the files
already present on PyPI before deciding how to recover; do not assume a rerun
can replace them. Use a new version for a rebuilt or changed release instead
of trying to overwrite published artifacts.

The workflow's `pypi` environment reference and PyPI publisher configuration
must match exactly. If authentication or approval fails, check the one-time
setup rather than adding credentials to the build jobs. See the
[PyPA publishing action documentation](https://github.com/pypa/gh-action-pypi-publish).
