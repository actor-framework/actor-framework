.. _faq:

Frequently Asked Questions
==========================

This Section is a compilation of the most common questions via GitHub, chat,
and mailing list.

Can I Encrypt CAF Communication?
--------------------------------

Yes, by using the OpenSSL module (see :ref:`free-remoting-functions`).

Can I Create Messages Dynamically?
----------------------------------

Yes.

Usually, messages are created implicitly when sending messages but can also be
created explicitly using ``make_message``. In both cases, types and number of
elements are known at compile time. To allow for fully dynamic message
generation, CAF also offers ``message_builder``:

.. code-block:: C++

   message_builder mb;
   // prefix message with some atom
   mb.append(strings_atom::value);
   // fill message with some strings
   std::vector<std::string> strings{/*...*/};
   for (auto& str : strings)
     mb.append(str);
   // create the message
   message msg = mb.to_message();

What Debugging Tools Exist?
---------------------------

The ``scripts/`` directory contains some useful tools to aid in analyzing CAF
log output.
