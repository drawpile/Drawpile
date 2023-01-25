#!/usr/bin/env bash

set -ueo pipefail

rm -rf ~/.cargo/registry/index/github.com-1ecc6299db9ec823/.git
git clone --depth 1 --no-checkout \
	https://github.com/rust-lang/crates.io-index \
	~/.cargo/registry/index/github.com-1ecc6299db9ec823
