PYTHONPATH=../../dpcore/src/protocol python3 -B ./protogen-builder.py | rustfmt > messages_ffi.rs && echo "Regenerated messages_ffi.rs"

