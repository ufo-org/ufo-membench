# ufo-membench

A memory benchmark for UFOs.

## Building

Before building, retrievew the code of submodules:

```bash
git submodule update --init --recursive
```

To update the submodules, pull them.

```bash
cd src/ufo_c && git pull origin events && cargo update && cd ../.. 
```

Then build all packages:

```bash
make
```

## Troubleshooting

```
thread '<unnamed>' panicked at 'called `Result::unwrap()` on an `Err` value: SystemError(Sys(EPERM))', .../src/ufo_core.rs:215:14
```

System is set up only to allow root use `userfaultfd`. Fix by setting system
priivileges to allow unprivileged users to use `userfaultfd`:

```bash
sysctl -w vm.unprivileged_userfaultfd=1
```
