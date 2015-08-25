Squinting at Power Series
=========================

This code is a translation of the code in
[Squinting at Power Series](http://swtch.com/~rsc/thread/squint.pdf)
into C using libthread.

There are a few differences:

* The paper uses a queue of threads to split a series.  This code uses
two threads each with a queue data structure.
* The paper uses queues of threads to multiply two series.  This code
maintains two queues of values in one thread.
* The code for power series reversion is based on the simpler formula
given in McIlroy's later paper [Power Series, Power
Serious](http://www.cs.dartmouth.edu/~doug/pearl.ps.gz).
* Newsqueak is presumably garbage collected.  In this implementation
each power series has an extra channel `ps->close` which is
used to signal to the thread providing the values for the series to
free all its data structures and exit.  The handling needed for that
channel likely doubles the size of the code.

I have implementations of the original method of handling
multiplication and splitting, email me if you are interested.
