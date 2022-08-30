# sreutils

The UNIX holy trinity, redesigned with Rob Pike's
[structural regular expressions](http://doc.cat-v.org/bell_labs/structural_regexps/)
in mind.

## Programs

### siv

`grep` like program (Multi-layer regular expression matching).

### pike

`sed` replacement with `sam` syntax

### auk

`awk` implementation with structural regex support

## TODO

- add greedy and non-greedy matches
- rewrite `siv`
- refactor build and test systems
- write `pike`
- write `auk`

## License

All my code is MIT licensed. The code in `lib` is from
[plan9port](https://9fans.github.io/plan9port/unix)
and includes it's appropriate license.
