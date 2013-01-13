Omegle plugin for BitlBee
=========================
This is a simple plugin to talk on Omegle through your favourite IRC client.

When you add an account with the omegle protocol give as user and password whatever you want,
they are not used.

Add a handle with the name you want and you will be ready to talk to strangers.

Now, to start talking to a stranger you can either `allow stranger` or start sending messages to it.

To disconnect from him just `block stranger`, or tell him you're male, it has the same effect.

If you want the contact to stay online, set the option `keep_online` to true, it will set as away
the contact when not connected, and as online when connected.

If the contact is offline you can still talk to him or `allow` him to start talking, it will disconnect
when the partner disconnects.

By default the plugin adds a buddy called `Stranger`, you can change the prefix and the quantity with the
`stranger_prefix` and `auto_add_strangers` options.

You can also use CTCPs, there is CONNECT and DISCONNECT CTCPs, that well, do what they are called.

If you want you can set the common topics to look for in the next stranger by sending a LIKES CTCP with a space
separated list of topics. If the look for the stranger is taking too long and you're bored you can tell it to
look for random strangers by sending an empty LIKES packet, remember to reset the LIKES after the completely
random stranger though.

Usage
-----

```
account add omegle <username> whatever
```

Building and Installing
-----------------------

```
$ git clone https://github.com/meh/bitlbee-omegle.git
$ cd bitlbee-omegle
$ autoreconf -fi
$ ./configure
$ make
# make install
```
