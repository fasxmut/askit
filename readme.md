Askit
======================================================================

Askit many c++ commands to processing data and network request.

Requirements:

c++ compiler

boost libraries, boost filesystem, boost process, boost asio, boost beast

botan c++ libraries

#### Please enable c++ reflection and contracts,

The project itself does not take the responsibility for enabling them.

Enable them at `/etc/site-config.jam` :

```
using gcc : : : <cxxflags>"-freflection -fcontracts" <linkflags>"-fcontracts" ;

project
:
	default-build
		<cxxstd>26
;
```

### Build:

```
b2
```

### Install:

```
b2 install --prefix=/path/to	# default: /usr/local
```

### cleanall:

```
b2 cleanall
```

Sopv
----------------------------------------

Sopv c++ network request client command. Sopv is a c++ network request application written in c++ boost asio beast and botan cryptographic libraries.

Sdv1
----------------------------------------

Sdv1 is a command written in c++ to transform data.

Asidt
----------------------------------------

Simple Indent

Dmm
----------------------------------------

Dear Manager of Process: dmm is a c++ program to manage process.

Dperm
----------------------------------------

Change permissions of many files to default permissions, written in c++ std::filesystem.

#### Dperm Disclaimer:
* Do not use this program to change system files permissions, such as /etc, /bin ; or take it at your own risk .
* Developer(s) does not take any responsibility for any damage or loss .

Xcmp
----------------------------------------

Compare two strings or numbers, the result is one of:

* eq	-	equal
* lt	-	less than
* gt	-	greater than
* na	-	not comparable

Rules:

* The string is number if:
	* Required: Any place character is digit or dot
	* Required: The string contains maximum one dot
	* Optional: The staring starts with + or -
* If both inputs are numbers, compare them as double.
* No matter how many zero the number starts with, the number is always decimal.

Examples:

```
$ xcmp c++ c++
eq
$ xcmp 0.1122334455 0.1122334455
eq
$ xcmp "0.1122334455" 0.1122334455
eq
$ xcmp "0.112233445" "0.1122334455"
lt
$ xcmp 5 2
gt
$ xcmp 5.0 5
eq
$ xcmp abCd A
gt
$ xcmp 000009 9.0000000000000000000
eq
$ xcmp 0.9 .9
eq
$ xcmp 9. 9
eq
```

