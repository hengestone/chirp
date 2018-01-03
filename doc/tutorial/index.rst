=======================================
Tutorial - a simple monitoring solution
=======================================

This tutorial is intended to give you a first glimpse into the way that Chirp
works, and what it can do.

To this end, we build up a (very, very simple) system monitoring tool. The goal
is to have a few instances of a program called an "agent", which periodically
checks the state of a service (such as a webserver).  Then, we have a central
system that displays the state of all the services.

As an extension, we also implement an intermediate program that can gather the
collected information and forward it. This could be useful in a secure networking
environment where you don't want to allow every agent to connect to the outside.

Now you could easily monitor a medium-sized webapp deployment with some web
servers, database servers and load balancers. Assuming the web and database
servers are within a protected network, the monitoring architecture could look
something like this:

.. graphviz::

   digraph Monitoring {
        rankdir=LR;
        graph[splines=polyline];

        "Web Server 1" [shape=parallelogram];
        "Web Server 2" [shape=parallelogram];
        "Web Server 3" [shape=parallelogram];
        "Web Server 4" [shape=parallelogram];
        "DB Server"        [shape=parallelogram];
        "DB Server Backup" [shape=parallelogram];

        "Load Balancer 1" [shape=parallelogram];
        "Load Balancer 2" [shape=parallelogram];

        "Collector" [shape=cds];

        "Monitor" [shape=box];

        // Protected services forward their events to a collector
        "Web Server 1"     -> "Collector";
        "Web Server 2"     -> "Collector";
        "Web Server 3"     -> "Collector";
        "Web Server 4"     -> "Collector";
        "DB Server"        -> "Collector";
        "DB Server Backup" -> "Collector";

        "Collector" -> "Monitor"  [label="Collector forwards events from the protected env."];

        "Load Balancer 1" -> "Monitor";
        "Load Balancer 2" -> "Monitor" [label="LB can be monitored from the outside"];
   }

Getting started
===============

To get started, we copy the source distribution of chirp into a new work
directory. To do this, copy the following files to some directory.

* ``libchirp.h``
* ``libchirp.c``
* ``libchirp-config.h``

Then, you create three additional files that will be the components of our
monitoring solution. I've linked the source code, so if you're impatient, you
can just grab the files and continue straight on.

* :download:`common.h <common.h>` - a header with some definitions that we're going to reuse
* :download:`agent.c <agent.c>` - the agent that monitors the services
* :download:`collector.c <collector.c>` - collects messages from multiple agents
* :download:`monitor.c <monitor.c>` - displays the status of each monitored service

We'll fill those with code in a minute.

To help you get started, we've also created a :download:`Makefile <Makefile>`.
for you that you can use as a starting base. If you prefer to roll your own,
you can compile the code with the following GCC flags:

.. code-block:: bash

   gcc agent.c libchirp.c -o agent -lssl -luv -lm -lpthread -lcrypto
   gcc monitor.c libchirp.c -o monitor -lssl -luv -lm -lpthread -lcrypto
   gcc collector.c libchirp.c -o collector -lssl -luv -lm -lpthread -lcrypto

But with the Makefile in place, just run ``make``, and everything will be
built. Of course, at this point in time, this won't work just yet (unless you
cheated and downloaded the source files already!).

How to get libchirp
-------------------

Either download a release of libchirp or see README.rst_ in the git repository.

To build the source distribution from git call:

.. _README.rst: https://github.com/concretecloud/chirp#how-to-create-a-source-distribution

.. code-block:: bash

   make clean dist

In libchirp-config.h you can enable logging and asserts uncommenting the
following two defines.

.. code-block:: bash

   /* #define CH_ENABLE_LOGGING */
   /* #define CH_ENABLE_ASSERTS */

Making a plan
=============

Okay, before we start for real, let's think for a second and decide what each
part should actually *do*! We want to monitor network services, which all serve
TCP on a given IP address and port. Each component will use Chirp as a means to
communicate with each other.

Since an agent can report to a monitor or a collector, we will call the
peer of an agent "upstream". A collector's upstream can in turn be a collector,
or the "final" monitor.

For simplicity, we limit the service names to 32 chars, and put some more
restrictions out, which would need to be removed for a full-blown monitoring
solution. But for the scope of this tutorial, it's more than enough.

Monitor
-------


Agent
-----

The next part will be the agent. An agent is a stand-alone program that
periodically checks if another service is still around and healthy, and reports
the result upstream. We'll implement one simple check that checks if a TCP
port is accepting connections. Of course, you can implement more complex
agents, or start building upon the huge collection of nagios/icinga checks if you want.

The agent will repeatedly try to connect to the TCP port specified on the
commandline, and report the result upstream. If the agent quits
(:kbd:`Ctrl-C`), it will let the upstream know that the service is not being
monitored anymore.

Collector
---------

The collector is in principle the simplest part of all: It receives messages from
the agents, then forwards them to the monitor. In theory, you could chain a number
of collectors together if needed.

Communications
--------------

One last thing: Let's define how the parts talk to each other. A monitoring
solution always needs to represent a service and it's state. Of course, not
only the service can go down, but the agent itself as well. So we can't get
around the fact that the agent needs to report it's own state, and let the
monitor know if it's coming or going.

Since the monitor itself is informed about the services under check by the
agents, it's the agent's responsibility to tell the monitor when a service is
de-monitored. An agent that goes offline regularly will actively de-monitor
it's service(s) first.

We want to keep things simple, thus the monitor won't really know about the
whole collector structure. 

This gives us the following message types:

* Service status update: Give information about a service's status
* Un-monitor a service: Let the monitor know the service is not being monitored
  anymore

We don't do an explicit "add": If we get word of a service, we add it to the
list (if it's not yet there).

Now, the code
=============

.. toctree::
   :maxdepth: 2

   common.h.rst
   monitor.c.rst
   agent.c.rst
   first_tests.rst
   collector.c.rst
