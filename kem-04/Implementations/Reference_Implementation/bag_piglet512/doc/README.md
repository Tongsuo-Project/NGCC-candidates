# API documentation

This instance includes a `doxygen.conf` file for generating browsable source
documentation from the C headers and implementation files.

From the instance directory, run:

```sh
doxygen doxygen.conf
```

The generated output directory is controlled by `doxygen.conf`. Doxygen output
is a local build artifact and is not tracked by the repository.

For the implementation overview, build instructions, official KAT boundary,
and local-test policy, read the instance-level `README` and the repository
root `README.md`.
