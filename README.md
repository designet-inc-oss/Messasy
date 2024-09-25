# Messasy (Message Archive System)
##  What is Messasy
Messasy is a software that saves e-mails sent through MTA
in cooperation with MTA via MILTER interface.

## Requirements
The following softwares are required.

* Sendmail (8.10 or later) or Postfix (2.3 or later)
* libmilter (included to Sendmail)
* libdg
  + https://github.com/designet-inc-oss/libdg

## Download
https://github.com/designet-inc-oss/Messasy

## Installation

```
./configure
make
make install
```

## for developers
If you want to redo autoconf/automake, run:

```
make distclean-all
./INITIAL
```

## Homepage
https://www.designet.co.jp/open_source/messasy

## Bug reports to
https://github.com/designet-inc-oss/Messasy/issues
