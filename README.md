# Project 2 - CSC4200

## Getting started

To make development easier, I have converted the pintos VM into a docker container. 

With docker installed, run

``` bash
make docker
```

## Running tests

``` bash
make docker-check
```

Tests that need to pass:
- tests/threads/priority-change
- tests/threads/priority-preempt
- tests/threads/priority-fifo
- tests/threads/priority-sema
- tests/threads/priority-condvar
- tests/threads/priority-donate-one
- tests/threads/priority-donate-multiple
- tests/threads/priority-donate-multiple2
- tests/threads/priority-donate-nest
- tests/threads/priority-donate-chain
- tests/threads/priority-donate-sema
- tests/threads/priority-donate-lower


## Running threads

``` bash
make run-threads
```

## Other targets

Just look at the Makefile