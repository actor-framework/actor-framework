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

  interface base [role: upper layer] {
    /// Called whenever the underlying transport is ready to send, allowing the
    /// upper layers to produce additional output data or use the event to read
    /// from event queues, etc.
    /// @returns `true` if the lower layers may proceed, `false` otherwise
    ///          (aborts execution).
    template <class LowerLayerPtr>
    bool prepare_send(LowerLayerPtr down);

    /// Called whenever the underlying transport finished writing all buffered
    /// data for output to query whether an upper layer still has pending events
    /// or may produce output data on the next call to `prepare_send`.
    /// @returns `true` if the underlying socket may get removed from the I/O
    ///          event loop, `false` otherwise.
    template <class LowerLayerPtr>
    bool done_sending(LowerLayerPtr down);
  }

  interface base [role: lower layer] {
    /// Returns whether the layer has output slots available.
    bool can_send_more() const noexcept;
  }

  interface stream_oriented [role: upper layer] {
    /// Called by the lower layer for cleaning up any state in case of an error.
    template <class LowerLayerPtr>
    void abort(LowerLayerPtr down, const error& reason);

    /// Consumes bytes from the lower layer.
    /// @param down Reference to the lower layer that received the data.
    /// @param buffer Available bytes to read.
    /// @param delta Bytes that arrived since last calling this function.
    /// @returns The number of consumed bytes. May be zero if waiting for more
    ///          input or negative to signal an error.
    /// @note When returning a negative value, clients should also call
    ///       `down.abort_reason(...)` with an appropriate error code.
    template <class LowerLayerPtr>
    ptrdiff_t consume(LowerLayerPtr down, byte_span buffer, byte_span delta);
  }

  interface stream_oriented [role: lower layer] {
    /// Configures threshold for the next receive operations. Policies remain
    /// active until calling this function again.
    /// @warning Calling this function in `consume` invalidates both byte spans.
    void configure_read(read_policy policy);

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

    /// Propagates an abort reason to the lower layers. After processing the
    /// current read or write event, the lowest layer will call `abort` on its
    /// upper layer.
    void abort_reason(error reason);

    /// Returns the last recent abort reason or `none` if no error occurred.
    const error& abort_reason();
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
    template <class LowerLayerPtr>
    ptrdiff_t consume(LowerLayerPtr down, byte_span buffer);
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
    template <class LowerLayerPtr>
    ptrdiff_t consume(LowerLayerPtr down, byte_span buffer);
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
    /// @note When returning `false`, clients must also call
    ///       `down.set_read_error(...)` with an appropriate error code.
    template <class LowerLayerPtr>
    bool end_message();
  }

