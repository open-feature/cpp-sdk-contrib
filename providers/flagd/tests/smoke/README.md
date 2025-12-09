# Smoke testing flagd provider

## Grpc sync test

To run *sync test* you will need to initialize flagd on your localhost, over some sync structure (preferably file for easy manipulation). F. e.:
```
./bin/flagd start --port 8013 --uri file:./example_flags.json
```

Having empty flagd definition in `./example_flags.json`:
```
{
  "$schema": "https://flagd.dev/schema/v0/flags.json",
  "flags": {}
}
```


With flagd running you should invoke the test by:
```
bazel run providers/flagd/tests/smoke:grpc_sync
```

You should see info every two seconds:
```
Flag not found (yet).
```

The test is simply looking for any flag with the key: `myBoolFlag`.

That's why, after changing `./example_flags.json` file contents to:
```
{
  "$schema": "https://flagd.dev/schema/v0/flags.json",
  "flags": {
    "myBoolFlag": {
      "state": "ENABLED",
      "variants": {
        "on": true,
        "off": false
      },
      "defaultVariant": "off"
    }
  }
}
```

You should observe that the info produced by the test changed to:
```
Flag found!
```

Which validates that the syncing between provider and flagd instance is working correctly.