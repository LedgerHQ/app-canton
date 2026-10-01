# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [3.5.1] - 2026-09-29

### Fixed

- Clear signing for a USDCx send that merges several holdings. The app ran out of room in memory
  and refused it with 0xB005.
- Stop clear signing when a screen value, such as a long memo, cannot be stored. The value used to
  vanish from the screen.

## [3.5.0] - 2026-09-22

### Added

- Clear signing for Token Standard V2 transfers: transfer, accept, reject and withdraw.
- Clear signing for consolidating your own holdings, which is a transfer to yourself.

### Fixed

- Clear signing when you send a registry token such as CBTC or USDCx. The app did not know the
  contract the registry uses to start a transfer, so it fell back to blind signing.

### Changed

- Update the transaction protobuf definitions to v1.6.3.

## [3.4.0] - 2026-09-18

### Added

- Check that every node of a prepared transaction belongs to the transaction shown on screen.
- Check that value only reaches the account, and only in the amount, shown on screen.

### Fixed

- Refuse onboarding that carries more than one signing key, or signing keys the device never shows.
- Choose the clear-signing screen from the node's own template and choice, not from a record found
  inside it.
- Bind the accept, reject and withdraw screens to the contract the transaction exercises.
- Refuse prepared-transaction nodes that leave out a required field.
- Return a status word instead of aborting the app on a malformed onboarding sequence.
- Reject SIGN_TX commands with invalid parameters before they change any state.
- Wipe key material from stack buffers after use.
- Free parser and display memory when a transaction is refused.

### Changed

- Pin CI Python dependencies to exact versions.
- Add a CODEOWNERS file.

## [3.3.5] - 2026-08-18

### Changed

- Remove single validator topology setups for onboarding, for all networks.

## [3.3.4] - 2026-07-22

### Changed

- Update Kiln main net validator party ID.

## [3.3.3] - 2026-07-10

### Fixed

- Various minor fixes.

### Changed

- Update devnet and testnet validators.
- Remove clear signing for Featured app proxy transactions.
- Update tests and snapshots.

## [3.3.1] - 2026-02-27

### Fixed

- Fixing non-standard types prior to clang-21 migration

## [2.1.0] - 2023-10-06

### Changed

- Improving the settings use case in order to be able to use app settings parameters stored in NVM
- add a NBGL use case choice when a setting switch is toggled

## [2.0.0] - 2023-07-10

### Added

- Stax porting
- Extensive CI, including mandatory `guidelines_enforcer.yml`
- Extensive `README.md` to modify/compile/test the application on most OS (Linux, MacOS, Windows)
- Extensive `Ragger` tests

### Changed

- Simplified `Makefile` (complexity delegated to the SDK's `Makefile.standard_app`)
- Simplified overall code (moved into the SDK)
- Improving several UI flows to fit Ledger UI guidelines
- Removing `TRY`/`CATCH` usage (using `_no_throw` SDK functions)
- Cleaning unnecessary resources (moved into the SDK)

### Fixed

- Multiple minor lint, prototype or misspell fixes

## [1.0.1] - 2021-01-11

### Fix

- Missing header includes

## [1.0.0] - 2020-11-19

### Added

- Initial commit with the brand new Boilerplate application
