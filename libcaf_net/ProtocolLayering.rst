Protocol Layering
=================

Layering plays an important role in the design of the ``caf.net`` module. When
implementing a new protocol for ``caf.net``, this protocol should integrate
naturally with any number of other protocols. To enable this key property,
``caf.net`` establishes a set of conventions and abstract interfaces.

Concepts
--------

A *protocol layer* is a class that implements a single processing step in a
communication pipeline. Multiple layers are organized in a *protocol stack*.
Each layer may only communicate with its direct predecessor or successor in the
stack.

At the bottom of the protocol stack is usually a *transport layer*. For example,
a ``stream_transport`` manages a stream socket and provides access to input and
output buffers to the upper layer.

At the top of the protocol is an *application* that utilizes the lower layers
for communication via the network. Applications should only rely on the
*abstract interface type* when communicating with their lower layer. For
example, an application that processes a data stream should not implement
against a TCP socket interface directly. By programming against the abstract
interface types of ``caf.net``, users can instantiate an application with any
compatible protocol stack of their choosing. For example, a user may add extra
security by using application-level data encryption or combine a custom datagram
transport with protocol layers that establish ordering and reliability to
emulate a stream.

Abstract Interface Types
------------------------

By default, ``caf.net`` distinguishes between these abstract interface types:

* *stream*: A stream interface represents a sequence of Bytes, transmitted
  reliable and in order.
* *datagram*: A datagram interface provides access to some basic transfer units
  that may arrive out of order or not at all.
* *message*: A message interface provides access to high-level, structured data.
  Messages consist of a header and a payload. A single message may span multiple
  datagrams, for example.

Note that each interface type also depends on the *direction*, i.e., whether
talking to the upper or lower level. Incoming data always travels the protocol
stack *up*. Outgoing data always travels the protocol stack *down*.

..code-block:: C++

  interface stream_oriented [role: upper layer] {
    /// Consumes bytes from the lower layer.
    /// @param down Reference to the lower layer that received the data.
    /// @param buffer Available bytes to read.
    /// @param delta Bytes that arrived since last calling this function.
    /// @returns The number of consumed bytes (may be zero if waiting for more
    ///          input or negative to signal an error) and a policy that
    ///          configures how many bytes to receive next (as well as
    ///          thresholds for when to call this function again).
    /// @note When returning a negative value for the number of consumed bytes,
    ///       clients must also call `down.set_read_error(...)` with an
    ///       appropriate error code.
    template <class LowerLayer>
    pair<ptrdiff_t, receive_policy> consume(LowerLayer& down, byte_span buffer,
                                            byte_span delta);
  }

  interface stream_oriented [role: lower layer] {
    /// Prepares the layer for outgoing traffic, e.g., by allocating an output
    /// buffer as necessary.
    void begin_output();

    /// Returns a reference to the output buffer. Users may only call this
    /// function and write to the buffer between calling `begin_output()` and
    /// `end_output()`.
    byte_buffer& output_buffer();

    /// Prepares written data for transfer, e.g., by flushing buffers or
    /// registering sockets for write events.
    void end_output();
  }

  interface datagram_oriented [role: upper layer] {
    /// Consumes a datagram from the lower layer.
    /// @param down Reference to the lower layer that received the datagram.
    /// @param buffer Payload of the received datagram.
    /// @returns The number of consumed bytes or a negative value to signal an
    ///          error.
    /// @note Discarded data is lost permanently.
    /// @note When returning a negative value for the number of consumed bytes,
    ///       clients must also call `down.set_read_error(...)` with an
    ///       appropriate error code.
    template <class LowerLayer>
    ptrdiff_t consume(LowerLayer& down, byte_span buffer);
  }

  interface datagram_oriented [role: lower layer] {
    /// Prepares the layer for an outgoing datagram, e.g., by allocating an
    /// output buffer as necessary.
    void begin_datagram();

    /// Returns a reference to the buffer for assembling the current datagram.
    /// Users may only call this function and write to the buffer between
    /// calling `begin_datagram()` and `end_datagram()`.
    /// @note Lower layers may pre-fill the buffer, e.g., to prefix custom
    ///       headers.
    byte_buffer& datagram_buffer();

    /// Seals and prepares a datagram for transfer.
    void end_datagram();
  }

  interface message_oriented [role: upper layer] {
    /// Consumes a message from the lower layer.
    /// @param down Reference to the lower layer that received the message.
    /// @param buffer Payload of the received message.
    /// @returns The number of consumed bytes or a negative value to signal an
    ///          error.
    /// @note Discarded data is lost permanently.
    /// @note When returning a negative value for the number of consumed bytes,
    ///       clients must also call `down.set_read_error(...)` with an
    ///       appropriate error code.
    template <class LowerLayer>
    ptrdiff_t consume(LowerLayer& down, byte_span buffer);
  }

  interface message_oriented [role: lower layer] {
    /// Prepares the layer for an outgoing message, e.g., by allocating an
    /// output buffer as necessary.
    void begin_message();

    /// Returns a reference to the buffer for assembling the current message.
    /// Users may only call this function and write to the buffer between
    /// calling `begin_message()` and `end_message()`.
    /// @note Lower layers may pre-fill the buffer, e.g., to prefix custom
    ///       headers.
    byte_buffer& message_buffer();

    /// Seals and prepares a message for transfer.
    void end_message();
  }

