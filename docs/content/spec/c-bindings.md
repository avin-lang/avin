---
title: C bindings
summary: Avin calls a C library directly, through bindings written in Avin whose C prototypes clang checks against the library's own headers.
weight: 19
---

A C library reaches Avin through a **C binding**: an Avin declaration that copies the C function's
prototype from the library's header and states the signature Avin code calls it with, in Avin
types. The compiler calls the C function directly from Avin's own code, converting each argument and
the result at the call, and links the library the package's manifest names.

```avin
module mlt

@c(type: "mlt_properties", release: "mlt_properties_close", retain: "mlt_properties_inc_ref")
class Properties
    @c("char *mlt_properties_get(mlt_properties self, const char *name)")
    get(name: string) -> Option[string]

    @c("int mlt_properties_set(mlt_properties self, const char *name, const char *value)")
    set(name: string, value: string) -> Result[unit, int]

@c("mlt_properties mlt_properties_new(void)")
newProperties() -> Option[Properties]
```

A binding is a declaration without a body, as a [Sysl binding](/spec/sysl-bindings/) is: a top-level
function, or a method of a C handle. `@c` takes the prototype as one string, written as the header
writes it, parameter names optional; the C parameters correspond to the Avin parameters **by
position**, the receiver first for a method. Bindings are usually private, with a public function
giving the Avin-shaped API. A binding is otherwise an ordinary function: it is called, overloaded,
given defaults and passed as a value like any other.

A prototype the header does not declare, or declares otherwise, is rejected at the binding with
clang's own words:

```text
error: the C binding `get` does not match `mlt_properties_get` in framework/mlt.h: the header declares 'char *(mlt_properties, const char *)', but the binding says 'char *(mlt_properties, char *)'
```

## How a C binding is compiled

**A C binding is a direct call.** Settled on the user's question ("Why should C interop go through
Sysl since avin generates LLVM IR?"): Avin emits LLVM IR, and a C function is called from LLVM IR by
declaring its symbol and calling it. The [Sysl shim](/spec/sysl-bindings/#how-a-binding-is-compiled)
exists so that Sysl checks a binding against a Sysl signature; a C binding has no Sysl signature, so
a shim would add a compile step, a cache and a Sysl dependency and check nothing.

1. **Check** reads each prototype into a C signature — scalar types, pointers, `const`, the names of
   typedefs, function pointers — and holds it against the Avin signature by the rules below, so a
   binding whose Avin types cannot carry its C types is rejected before anything is built.
2. The driver generates one C file for the program, the **C unit**, and compiles it with clang,
   using the include flags of the [C libraries](#c-libraries) the program names. It holds:
   - **the header check**: for every binding, a `_Static_assert` that the function the header
     declares has exactly the type the prototype says (`__builtin_types_compatible_p`), and, for
     every typedef a prototype names, that it is the kind of type its Avin position needs (an
     integer, a pointer, a struct);
   - **the measurements**: the width and sign of each typedef in an integer position, the size,
     alignment and field offsets of each [mirrored struct](#structs), the value of each
     [enumerator and constant](#enums-and-constants), stored as data in the object file, which the
     driver reads without running it, so a build for another machine measures that machine;
   - **the glue**: a small C function for each thing that is a C *expression* rather than a function
     — a field read, a [`retain` or `release`](#handles) written as an expression, a
     [conversion between handle types](#handles-and-c-s-inheritance), a [buffer's
     length](#buffers), a struct [passed by value](#structs-by-value), a macro.
3. **Lower** gives each binding a body: convert the arguments, call the C symbol (or its glue),
   convert the result, as MIR the rest of the compiler already knows. **Emit** declares the C symbol
   (`declare i32 @mlt_properties_set(ptr, ptr, ptr)`) with the attributes the target's C convention
   asks for — `signext` or `zeroext` on an integer narrower than 32 bits, as the [Sysl
   boundary](/spec/sysl-bindings/#integer-parameters) already does.
4. **Link** passes the C unit's object and the libraries' link flags to clang with the program.

The C unit is cached by a hash of its source, the include and link flags `pkg-config` answers, and
`clang --version`. Nothing about a C binding needs the `sysl` command.

## C libraries

A package that binds a C library names it in its manifest, under `c-libraries`, with the
`pkg-config` module that finds it and the headers its bindings are checked against:

```hocon
c-libraries {
  mlt {
    pkg-config = "mlt-framework-7"
    headers    = ["framework/mlt.h"]
    install    = "MLT 7: `brew install mlt`; Debian's libmlt-dev"
  }
  libc { headers = ["<stdlib.h>", "<string.h>"] }
}
```

- `pkg-config` names the module whose `--cflags` the C unit is compiled with and whose `--libs` the
  program is linked with; both are asked at every build, so a library that moved is still found.
  `link = ["z"]` names libraries directly instead, each passed as `-l<name>`; an entry with neither
  needs nothing at the link (the C library itself, whose headers are named only to check against).
- `headers` are included by the C unit, in order, for every binding of the package. **A binding is
  checked against a header, always**: a package with a `@c` binding and no `headers` is refused.
- `install` is what a build that cannot find the module says to do:

  ```text
  error: the C library `mlt` of package `mlt` was not found: `pkg-config --cflags mlt-framework-7` failed. MLT 7: `brew install mlt`; Debian's libmlt-dev
  ```

**A program builds with the C libraries of every package in it**, as it does with their
[Sysl dependencies](/spec/packages/#sysl-dependencies), so a library that binds MLT works in a
program that never names MLT itself.

## Prototypes

The prototype is C's declaration with three words of its own, which say what C's types cannot. They
are written before the parameter's type and removed before the header check:

| Word | Meaning |
| --- | --- |
| `out` | A pointer C writes a value through; the value is part of the binding's [result](#arrays-and-out-parameters), and the parameter is not an Avin parameter |
| `inout` | A pointer C reads a value through and writes one back; an Avin parameter and part of the result |
| `userdata` | The `void *` that travels beside a [callback](#callbacks); not an Avin parameter |

`length(data)` before an integer parameter says it is the length of the array parameter `data`, and
it is filled in from the array rather than written by the caller.

A binding whose Avin result is `unit` discards the C result, whatever it is. Every other C result is
converted.

## Scalars

| C type | Avin type | Crossing |
| --- | --- | --- |
| `signed char`, `short`, `int`, `long`, `long long`, `intN_t`, `ssize_t`, `ptrdiff_t`, `intptr_t` and their `unsigned` forms, `size_t`, `uintN_t`, an integer typedef, an enum | any integer type | Converted with a check, in each direction, as a [Sysl integer](/spec/sysl-bindings/#integer-results) is: a value the other side cannot hold stops the program at the call, naming the C type by its sign and width |
| `unsigned char`, `uint8_t`, `char` | `byte` | By value. A C `char` as a value is a byte, whatever its sign on the machine |
| `bool`, `_Bool` | `bool` | By value |
| `double` | `real` | By value |
| `float` | `real` | A parameter is rounded to the nearest `float`, as C converts one; a result is widened exactly |

Avin has no unsigned integers, so a `size_t` or a `uint32_t` is a `long` (or an `int`, where the
values fit) and a negative value passed for one stops the program:

```text
avin: trap: value out of range for u64 (size_t) at main.avin:12:5
```

The width of `long`, `size_t` and each typedef is the target's, measured by the C unit; the check is
emitted for the widths measured. `long double`, `_Complex` types and vector types are refused.

A typedef crosses as the Avin type of its kind, which the C unit checks: an integer typedef as an
integer type, one an integer one byte wide as `byte`, a `bool` as `bool`, and a `float` or a `double`
as `real`. `signed char` crosses only as an integer type, and `char` only as `byte`. A result one past
what the binding's Avin type holds stops the program naming that type, as a Sysl binding's does:

```text
avin: trap: value out of range for long at main.avin:13:13
```

## Strings

- **A `const char *` parameter takes a `string`**, copied into NUL-terminated memory that lives for
  the call. A string holding U+0000 stops the program at the call, since C would read the NUL as its
  end:

  ```text
  avin: trap: the string passed to `name` of `set` holds a NUL at byte 3 at main.avin:9:5
  ```

- **A `char *` result, or a `const char *` one, is copied into a new `string`**, up to its NUL. The
  bytes must be UTF-8; text that is not stops the program at the call. A binding that wants the
  bytes as they are answers `Array[byte]` instead, copied up to the NUL, and nothing is checked.
- **C keeps its own string.** A string C answers is borrowed: it is copied and left as it was. One the
  caller must free is bound with `free:` naming the function that frees it, called once the copy is
  made: `@c("char *mlt_properties_serialise_yaml(mlt_properties self)", free: "free")`.
- A `char *` parameter, which C may write, does not take a `string`: it takes an
  [array](#arrays-and-out-parameters).

## Null

A pointer C may answer null is bound as an `Option`: null is `None`. Bound as the type itself, a null
result stops the program at the call:

```text
avin: trap: `mlt_profile_init` answered null at main.avin:4:15
```

A pointer parameter bound as an `Option` passes null for `None`. Strings, handles and arrays may each
be wrapped this way, and nothing else is.

## Status results

C reports most failures as an integer. A C integer result bound as `Result[unit, int]` is `Ok(())`
when it is zero and `Err(code)` otherwise. With [out parameters](#arrays-and-out-parameters), a status result
bound as `Result[T, int]` holds the out values in its `Ok`, and they are read only when the status is
zero. Other conventions — a negative error number, `-1` beside `errno` — are written in Avin over the
integer, and a public function maps the code to the library's own error type. Whether `errno` is read
by the binding is [proposed](/spec/decisions/#proposed).

## Handles

A C object Avin holds without seeing inside is a **handle**: a `class` declared with `@c(type: …)`
naming the C pointer type, and the function that gives the object back:

```avin
@c(type: "mlt_producer", release: "mlt_producer_close",
   retain: "mlt_properties_inc_ref(mlt_producer_properties(self))")
class Producer
    @c("int mlt_producer_get_length(mlt_producer self)")
    length() -> int
```

A handle is the [Sysl handle](/spec/sysl-bindings/#handles) over a C pointer, and keeps its rules:

- **Only a binding makes a handle**, and **each handle owns one reference** to its C object, given
  back by `release` exactly once: by `close()`, or by the collector's finalizer when the object is
  freed. Closing again does nothing; a closed handle used stops the program at the call.
- **`release` and `retain` name a C function of the handle's pointer, or a C expression in `self`**,
  compiled in the C unit's glue and type-checked there by clang.
- **A pointer C answers is owned by default**: the handle takes the reference the call gave. A
  binding whose pointer C only lends — MLT's `mlt_playlist_get_clip`, `mlt_tractor_get_track` — says
  `borrowed`, and the handle takes a reference of its own through `retain` before it is made. A
  handle with no `retain` cannot be borrowed, and the binding is refused, since nothing could keep
  the lent object alive:

  ```text
  error: the C binding `clip` lends a `Producer`, and `Producer` declares no `retain`: a lent object is kept only by a reference of its own
  ```

- **A handle passed to C is lent for the call.** C's own convention is that a pointer it is handed is
  borrowed, and most functions keep to it. One that **keeps** what it is handed past the call — MLT's
  producers keep a pointer to their profile and take no reference to it — is bound with `keeps:`
  naming the parameter: the handle the binding makes (or, for a method, the receiver) then holds the
  Avin object passed, so it is not collected while the holder lives. One that **takes** the reference
  it is handed is bound with `consumes:`, and the handle passed is closed by the call without
  `release`.
- **A handle belongs to one owner at a time**, checked at every use as a Sysl handle's is: C
  libraries rarely let two threads use one object at once. A class whose C object is safe to use from
  any thread is declared `shared`; its owner is not checked, and it is
  [sendable](/spec/actors/#what-may-be-sent). Whether a handle passed across threads hands its
  ownership on follows the [Sysl handle's rule](/spec/sysl-bindings/#handles).
- **A handle is compared by identity**, and has no `Display` of its own.

### Handles and C's inheritance

C libraries model inheritance by a struct whose first member is its parent: an MLT producer is a
service, and a service is a property bag. A handle declares the other C types it may be passed as,
each with the C function or expression that makes one:

```avin
@c(type: "mlt_playlist", release: "mlt_playlist_close",
   retain: "mlt_properties_inc_ref(mlt_playlist_properties(self))",
   as: ["mlt_producer = mlt_playlist_producer(self)",
        "mlt_service = mlt_playlist_service(self)",
        "mlt_properties = mlt_playlist_properties(self)"])
class Playlist
    @c("int mlt_properties_set_int(mlt_properties self, const char *name, int value)")
    setInt(name: string, value: int) -> Result[unit, int]
```

A handle is passed where a prototype names one of its `as` types, receiver or parameter, through
that conversion; the conversion lends, and the handle stays the owner. A method whose receiver's C
type is neither the handle's type nor an `as` type is refused at the method.

### Field accessors

A handle on a struct whose layout the header publishes reads and writes its fields through methods
bound with `field:`. The reader takes no parameters and answers the field; the writer takes one and
answers `unit`:

```avin
@c(type: "mlt_profile", release: "mlt_profile_close")
class Profile
    @c(field: "width")
    width() -> int
    @c(field: "frame_rate_num")
    frameRateNum() -> int
```

The field's C type is the header's, so its conversion is the [scalar](#scalars) one for that type; a
field that is a pointer is read as a string or a borrowed handle by the same rules as a result.

## Structs

### Structs by pointer

A struct C allocates and Avin reaches through a pointer is a [handle](#handles), its fields read
through [accessors](#field-accessors). A struct Avin makes and passes by pointer, for C to read or
fill, is a [mirror](#structs-by-value) passed as `const T *` (read) or `out T *` (filled, part of the
result); the pointer is to a copy that lives for the call.

### Structs by value

A struct passed by value is **mirrored** by a `data` type with one variant, as a [Sysl
struct](/spec/sysl-bindings/#mirrors) is. Its fields are matched by name, the Avin lowerCamelCase to
C's snake_case, or by a field's own `@c("name")`; every C field is mirrored, and each field is a
scalar, a `string` (`const char *`) or another mirror. The C unit asserts the size, alignment and
offsets the mirror implies against the header's struct, so a struct laid out otherwise on some target
stops that target's build at the mirror.

```avin
@c(struct: "mlt_color")
data Color
    Color(r: byte, g: byte, b: byte, a: byte)
```

A struct passed by value whose layout Avin does not mirror — a union, a struct with a bit-field or a
flexible array, one a library documents as opaque but passes by value — is an **opaque value**: a
class declared with `@c(value: "mlt_event_data")`, whose object holds a copy of the C value's bytes
and is passed and answered by value. Its methods are bindings taking the value as their receiver:

```avin
@c(value: "mlt_event_data")
class EventData
    @c("int mlt_event_data_to_int(mlt_event_data self)")
    toInt() -> int
```

**A call that passes or answers a struct by value goes through glue.** LLVM IR does not lower an
aggregate to the registers the platform's C convention names; clang does. So a binding with a struct
by value is called through a C function the C unit defines, which takes and answers the struct
through pointers and makes the real call, and clang classifies the aggregate for whatever target the
program is built for ([proposed](/spec/decisions/#proposed); a classification of Avin's own, per
target, in lowering, may replace it where a measurement says the extra call matters). Scalars,
pointers and structs by pointer need no classification and are called directly.

## Enums and constants

A C enum is an integer, and a binding may take or answer it as one. Mirrored, it is a `data` type
whose variants have no fields, each the enumerator of its words in snake_case after a prefix, or the
one its own `@c("name")` names:

```avin
@c(enum: "mlt_image_format", prefix: "mlt_image_")
data ImageFormat
    None
    Rgb
    Rgba
    Yuv422
    @c("mlt_image_yuv420p")
    Yuv420p
```

Each enumerator's value is measured by the C unit. A value C answers that no variant holds stops the
program at the call. A mirror need not list every enumerator: one it leaves out is a value it cannot
take.

A constant the header defines — an enumerator, a `#define`, a `static const` — is bound as a
top-level `val` without an initializer, measured by the C unit and folded into the program as a
constant:

```avin
@c(const: "LIBMLT_VERSION_INT")
val mltVersion: int
```

## Arrays and out parameters

- **An array parameter is a pointer.** An `Array[byte]` goes to a `uint8_t *`, `char *` or
  `unsigned char *`, an `Array[int]` to an `int32_t *` or `int *`, an `Array[long]` to an
  `int64_t *`, an `Array[real]` to a `double *`; the element types must be the same size and kind,
  and nothing is converted. A `const` pointer is only read; a pointer that is not `const` may be
  written, and what C writes is in the array after the call. Its length goes to the parameter
  marked `length(data)`, which is not an Avin parameter; a binding taking an array whose length
  nothing carries is refused, since C could read past its end.

  ```avin
  @c("ssize_t read(int fd, uint8_t *into, length(into) size_t count)")
  private readInto(fd: int, into: Array[byte]) -> long
  ```

- **An out parameter is part of the result.** The Avin result is the C result followed by each
  `out` and `inout` value in order, as a tuple; a C result bound as `unit` leaves the tuple without
  it, one value alone is not a tuple, and a status bound as `Result[T, int]` holds the rest in its
  `Ok`. An out value is a scalar, a string, a handle (owned, unless the binding says `borrowed`) or a
  mirror.

  ```avin
  @c("int mlt_service_get_frame(mlt_service self, out mlt_frame *frame, int index)")
  private getFrame(index: int) -> Result[Frame, int]
  ```

## Buffers

Pixels and samples are memory C owns, often megabytes of it, valid while the object that owns them
is unchanged. A binding answers such memory in one of two ways:

- **Copied**, as an `Array[byte]`: an `out` pointer to bytes named with `bytes:` and given a
  `length:`, a C expression over the call's parameters and out values, evaluated in the glue after
  the call. The array never reads C's memory.
- **Borrowed**, as a `ByteView` from `avin.c`, named with `view:` and given a `length:`: a handle on
  the C memory that reads it in place, bounds-checked — `length()`, `apply(i)`, `slice(from, until)`
  copying a range out, and `copy()` — and holds a reference to the handle that owns the memory, so
  the owner is not collected while the view is open. **A view is valid until its owner is used
  again**: any bound method of the owner, or its `close()`, closes every view it lent first, since a
  second call may free or replace the memory (MLT converts a frame's image in place). A closed view
  used stops the program ([proposed](/spec/decisions/#proposed)):

  ```text
  avin: trap: the `ByteView` of `Frame` was closed when `Frame` was used again at main.avin:22:17
  ```

```avin
@c(type: "mlt_frame", release: "mlt_frame_close",
   retain: "mlt_properties_inc_ref(mlt_frame_properties(self))")
class Frame
    @c("int mlt_frame_get_image(mlt_frame self, out uint8_t **buffer, inout mlt_image_format *format, out int *width, out int *height, int writable)",
       view: "buffer", length: "mlt_image_format_size(format, width, height, NULL)")
    private imageRaw(format: ImageFormat, writable: int) -> Result[(ByteView, ImageFormat, int, int), int]
```

A view is read-only. Writing into C's memory is not offered.

## Calls that wait

A C function that may wait — for a frame, for a thread, for input — is bound with `blocking`, as a
[Sysl one](/spec/sysl-bindings/#calls-that-wait) is: `@c("…", blocking)`. The call is bracketed by
the runtime's `avin_rt_enter_safe` and `avin_rt_leave_safe`, so a collection runs while it waits.
Every argument is converted first, and none points into the Avin heap during the call: a string's
copy is not on the heap, an array is copied into memory outside it and back after the call, and a
handle's C pointer is a plain value whose object the call's frame keeps alive. The result is
converted once the thread is running again.

A call that is not `blocking` is made with the thread running, so it holds up every collection for
as long as it runs, and may be passed pointers into the Avin heap — an array's elements — directly.

## Callbacks

C calls back through a function pointer with a `void *` beside it. A binding's parameter of
function-pointer type takes an Avin function, and the `void *` marked `userdata`, in the binding's
prototype and in the function pointer's own, carries it:

```avin
@c("mlt_event mlt_events_listen(mlt_properties self, userdata void *data, const char *id, void (*listener)(mlt_properties owner, userdata void *data, mlt_event_data event))",
   keeps: "listener")
on(event: string, @threaded listener: (Properties, EventData) -> unit)
```

- **The Avin function's parameters are the C function's, without the `userdata`**, in order, each
  crossing out of C as a result does: a pointer to a handle's type is a handle C lends, made through
  `retain`; and the function's answer crosses into C as a parameter does. A binding takes one
  function per `userdata`, and a function pointer with no `userdata` beside it is refused: there is
  nowhere to find the closure.
- **The compiler makes one trampoline per C function type**, a function with C's convention whose
  address is passed for the pointer. It finds the closure through the `userdata`, which points at the
  runtime's record of it (the record Sysl callbacks use, `callback_pin`), enters the runtime as a
  Sysl callback does (`callback_enter`), converts the arguments, calls the function, converts its
  answer and leaves. A callback with a struct by value among its parameters has a C trampoline in
  the glue that copies the struct out and calls the compiler's.
- **A callback C calls only during the binding** is released when the binding returns. **A callback C
  keeps** is bound with `keeps:` naming its parameter: the record is held by the receiver (or the
  handle the binding makes), so the closure lives as long as that handle. When the handle is
  released, its class's `forget:` expression is run for each callback it holds, with `userdata` the
  record's address, before `release` — MLT's `mlt_events_disconnect` — so a C object something else
  keeps alive cannot call into a freed record. A handle class that keeps callbacks and declares no
  `forget` is refused.
- **A callback belongs to the code that gave it**, and a call from any other thread stops the program,
  as for a Sysl callback. **A parameter marked `@threaded` takes a function any thread may call**,
  checked by its captures as a [Sysl `@threaded`
  parameter](/spec/sysl-bindings/#callbacks) is. A thread C started — MLT's consumer renders on one
  and fires `consumer-frame-show` there — is attached to the runtime for the call and detached after,
  so it may allocate, and a collection waits for it as for any running thread. A binding with a
  `@threaded` parameter is `blocking`.
- **C cannot say whether it calls from another thread**, as Sysl's `&sync Fn` does. A binding that
  leaves `@threaded` off a function C calls from its own thread is caught at run time, by the owner
  check, at the first such call.
- A trap inside a callback stops the program, reported at its own place, however deep in C.

## What is refused

| Refused | Instead |
| --- | --- |
| A variadic function (`...`) | A fixed-arity function the library offers, or a C wrapper the package carries ([proposed](/spec/decisions/#proposed)) |
| `void *` other than a callback's `userdata` | A handle on a typed pointer |
| A pointer type in Avin code | Handles, arrays, out parameters and views |
| A function pointer C answers | Bind the C functions it would point at |
| `long double`, `_Complex`, vector types, `va_list` | — |
| A binding with no header to check it against | Name the header in `c-libraries` |
| Writing into C's memory through a view | An array C fills |

## Soundness

**Avin has no pointer type and no `unsafe` block.** Every pointer lives inside a binding's
conversions, and safe Avin code sees only strings, arrays, handles, views and mirrors, each of which
is bounds-checked, owner-checked and closed-checked. **The binding declarations are the trusted
surface**, and the whole of it: what a reviewer reads to trust a library is the lines that begin
`@c(`, and the C glue they generate ([proposed](/spec/decisions/#proposed)).

What the compiler checks of that surface is what it can: the prototype against the header, by clang;
the Avin types against the prototype; every integer at run time; every typedef's kind and width and
every mirror's layout on the target built for. What it takes on trust is what no header says: that a
result is owned or `borrowed`, that a function `keeps` or `consumes` a pointer, that a buffer's
`length` is right, that C calls a function only from the thread a missing `@threaded` implies, and
that `forget` stops the callbacks. A wrong `borrowed` or `length` is a binding bug of the kind a wrong
`extern` is in C.

## An example: two clips rendered with MLT

The bindings, in the `mlt` package's own module (abridged to what the program uses):

```avin
module mlt

import avin.threads
import avin.time

data MltError
    UnknownService(kind: string, name: string)
    InvalidProfile(name: string)
    Failed(call: string, code: int)

@c(type: "mlt_profile", release: "mlt_profile_close")
class Profile
    @c(field: "width")
    width() -> int

@c("mlt_profile mlt_profile_init(const char *name)")
private profileInit(name: string) -> Option[Profile]

profile(name: string) -> Result[Profile, MltError] =
    profileInit(name).toRight(InvalidProfile(name))

@c(type: "mlt_properties", release: "mlt_properties_close", retain: "mlt_properties_inc_ref")
class Properties

@c(value: "mlt_event_data")
class EventData

@c(type: "mlt_producer", release: "mlt_producer_close",
   retain: "mlt_properties_inc_ref(mlt_producer_properties(self))",
   as: ["mlt_service = mlt_producer_service(self)"])
class Producer

@c("mlt_producer mlt_factory_producer(mlt_profile profile, const char *service, const char *resource)",
   keeps: "profile")
private factoryProducer(profile: Profile, service: Option[string], resource: string) -> Option[Producer]

producer(profile: Profile, resource: string) -> Result[Producer, MltError] =
    factoryProducer(profile, None, resource).toRight(UnknownService("producer", resource))

@c(type: "mlt_playlist", release: "mlt_playlist_close",
   retain: "mlt_properties_inc_ref(mlt_playlist_properties(self))",
   as: ["mlt_service = mlt_playlist_service(self)"])
class Playlist
    @c("int mlt_playlist_append_io(mlt_playlist self, mlt_producer producer, int in, int out)")
    private appendIoRaw(clip: Producer, from: int, to: int) -> Result[unit, int]

    appendIo(clip: Producer, from: int, to: int) -> Result[unit, MltError] =
        appendIoRaw(clip, from, to).mapErr(code -> Failed("mlt_playlist_append_io", code))

@c("mlt_playlist mlt_playlist_new(mlt_profile profile)", keeps: "profile")
private playlistNew(profile: Profile) -> Option[Playlist]

playlist(profile: Profile) -> Result[Playlist, MltError] =
    playlistNew(profile).toRight(Failed("mlt_playlist_new", 0))

@c(type: "mlt_consumer", release: "mlt_consumer_close",
   retain: "mlt_properties_inc_ref(mlt_consumer_properties(self))",
   as: ["mlt_properties = mlt_consumer_properties(self)"],
   forget: "mlt_events_disconnect(mlt_consumer_properties(self), userdata)")
class Consumer
    @c("int mlt_properties_set(mlt_properties self, const char *name, const char *value)")
    private setRaw(name: string, value: string) -> Result[unit, int]

    @c("int mlt_properties_set_int(mlt_properties self, const char *name, int value)")
    private setIntRaw(name: string, value: int) -> Result[unit, int]

    @c("mlt_event mlt_events_listen(mlt_properties self, userdata void *data, const char *id, void (*listener)(mlt_properties owner, userdata void *data, mlt_event_data event))",
       keeps: "listener")
    on(event: string, @threaded listener: (Properties, EventData) -> unit)

    @c("int mlt_consumer_connect(mlt_consumer self, mlt_service producer)", keeps: "source")
    private connectRaw(source: Playlist) -> Result[unit, int]

    @c("int mlt_consumer_start(mlt_consumer self)")
    private startRaw() -> Result[unit, int]

    @c("int mlt_consumer_is_stopped(mlt_consumer self)")
    private isStoppedRaw() -> int

    @c("int mlt_consumer_stop(mlt_consumer self)", blocking)
    private stopRaw() -> Result[unit, int]

    set(name: string, value: string) -> Result[unit, MltError] =
        setRaw(name, value).mapErr(code -> Failed("mlt_properties_set", code))

    setInt(name: string, value: int) -> Result[unit, MltError] =
        setIntRaw(name, value).mapErr(code -> Failed("mlt_properties_set_int", code))

    connect(source: Playlist) -> Result[unit, MltError] =
        connectRaw(source).mapErr(code -> Failed("mlt_consumer_connect", code))

    run() -> Result[unit, MltError] =
        startRaw().mapErr(code -> Failed("mlt_consumer_start", code))?
        while isStoppedRaw() == 0 do threads.sleep(time.milliseconds(10))
        stopRaw().mapErr(code -> Failed("mlt_consumer_stop", code))

@c("mlt_consumer mlt_factory_consumer(mlt_profile profile, const char *service, const char *input)",
   keeps: "profile")
private factoryConsumer(profile: Profile, service: string, input: string) -> Option[Consumer]

consumer(profile: Profile, service: string, target: string) -> Result[Consumer, MltError] =
    factoryConsumer(profile, service, target).toRight(UnknownService("consumer", service))
```

The program: two coloured clips, one second each, rendered to an MP4, counting the frames the
consumer shows from its render thread.

```avin
import mlt.{profile, playlist, producer, consumer, MltError}
import avin.threads.Atomic

renderTitle(file: string) -> Result[int, MltError] =
    val prof = profile("atsc_720p_25")?
    val show = playlist(prof)?

    show.appendIo(producer(prof, "color:#202020")?, 0, 24)?
    show.appendIo(producer(prof, "color:#c04000")?, 0, 24)?

    val out = consumer(prof, "avformat", file)?
    val frames = Atomic(0)

    out.set("vcodec", "mpeg4")?
    out.setInt("an", 1)?
    out.on("consumer-frame-show", (_, _) -> frames.add(1))
    out.connect(show)?
    out.run()?

    Ok(frames.load())
```

What each piece of the chapter does here: `profile` answers `None` for a null `mlt_profile_init`;
the producers and the playlist keep the profile, since MLT keeps only its pointer; `appendIo` lends
each producer, and MLT takes its own reference, so the producers made inline are collected without
leaving the playlist; the listener is `@threaded`, captures only a `val` holding an `Atomic`, and is
called on MLT's render thread, attached for each call; the consumer keeps it until it is released,
and `forget` disconnects it first; `run` waits by polling, each `sleep` safe for the collector, and
`stop`, which joins the render thread, is `blocking`.

## Relationship to Sysl bindings

The two kinds of binding sit side by side, and a program may use both. A library Sysl already binds
well is reached through Sysl; a C library is bound directly. The Sysl boundary's rules are the C
boundary's wherever both have the same question — integer conversions, `Option`, handles and their
owners, `blocking`, callbacks and `@threaded` — and differ where C has no Sysl signature to check
against: the header is checked instead, by clang, and what no header says is written in the binding.
