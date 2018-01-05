#!/usr/bin/env python3

from base64 import b64encode
import ed25519
import json

signing_key, verifying_key = ed25519.create_keypair()

print ("Signing key:", b64encode(signing_key.to_bytes()).decode('utf-8'))
print ("Verifying key:", b64encode(verifying_key.to_bytes()).decode('utf-8'))

sample_token = {
    "username": "bob",
    "flags": ["mod", "host"],
    "nonce": "0102030405060708",
    }

serialized_token = b64encode(json.dumps(sample_token).encode('utf-8'))

signature = signing_key.sign(b'1.' + serialized_token, encoding="base64")

print ("Auth token version: 1")
print ("Payload: " + serialized_token.decode('utf-8'))
print ("Signature: " + signature.decode('utf-8'))
print ("Token: 1." + serialized_token.decode('utf-8') + "." + signature.decode('utf-8'))

