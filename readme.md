Squinting at Power Series
=========================

This code is a translation of the code in
[Squinting at Power Series](http://swtch.com/~rsc/thread/squint.pdf)
into C using the concurrent extensions to KenCC.

There are a few differences:

* The paper uses a queue of threads to split a series.  This code uses
two threads each with a queue data structure.
* The paper uses queues of threads to multiply two series.  This code
maintains two queues of values in one thread.
* The implementation of power series reversion is based on the simpler formula
given in McIlroy's later paper [Power Series, Power
Serious](http://www.cs.dartmouth.edu/~doug/pearl.ps.gz).
* Newsqueak is garbage collected.  This implementation instead
gives each power series a channel `ps->close` which is
used to signal to the thread providing the values for the series to
free all its data structures and exit.

I had implementations of the original method of handling
multiplication and splitting, email me if you are interested I might
be able to find them.
