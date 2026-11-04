---
title: Derivation through shapes
summary: Library-defined derivation, where the compiler describes each data type and class as a shape, and a trait's own module says once how to derive it, checked at compile time and compiled per type.
weight: 25
---

Any trait can be derived. Besides the core library's `Eq`, `Hash`, `Ord`, `Display` and `Debug`, a
trait's module writes a **derive template** once, against the **shape** the compiler gives every
data type and class: its variants, their fields, each field's name, position, default and
annotations. A type that says `derives JsonEncoder` gets an ordinary impl, compiled for that type
alone, whose calls on the type's fields are written for each field's own type and look nothing up
at run time.

The aim is that a library such as a JSON codec is as complete written this way as written by hand:
encoder and decoder traits, derivation for data types and classes, renamed and omitted fields,
defaults, a discriminator for sum types, errors that name the path to the bad value, and a decoder
that streams. The [worked example](#a-json-library) below is that library's core.

## The approach

Generic code over a type's fields needs, for each field, a call at that field's own type, so that
`string` fields encode as strings and `int` fields as numbers, each call direct. Avin has neither
higher-kinded types nor type-level lists, and macros are deferred, so the question is who writes
those calls.

| Approach | What it needs | Verdict |
| --- | --- | --- |
| **The compiler walks the fields, calling the library's hooks** | A hook method generic over one type parameter; the compiler calls it once per field, at the field's type | **Chosen** |
| A type is converted to a tuple of its fields, and the library writes instances over tuples | Code generic over a tuple's arity, which Avin does not have: one instance per arity, names and annotations carried beside, an intermediate tuple per conversion | Rejected |
| The template loops over the fields, the loop unrolled per type | A name for "this field's type" inside the loop (a path-dependent type) and a body checked anew at every expansion: a macro by another name | Rejected |
| Descriptions read at run time | Lookups by name and boxing on every call | Rejected: cost the call site does not show |

The template is checked once, generically, like any generic code. What is checked for each deriving
type is the hook: it is found on the visitor type the template names, and each field's type must
meet the bound it declares, a failure reported at the `derives` entry, naming the field.

## Shapes

The core library declares the descriptions, as plain data:

```text
data TypeShape(name: string, variantCount: int, variants: Vector[VariantShape],
               annotations: Vector[Annotation])
data VariantShape(name: string, index: int, fieldCount: int, fields: Vector[FieldShape],
                  annotations: Vector[Annotation])
data FieldShape(name: string, index: int, hasDefault: bool, annotations: Vector[Annotation])

trait Annotation

impl FieldShape                                 // and the same on TypeShape and VariantShape
    annotation[A: Annotation]() -> Option[A]    // the first annotation of type A
    has(a: Annotation) -> bool                  // whether a is among them, by ==
```

- **A data type's shape** has one variant per variant, in declaration order, each with its fields.
  The single-line form has one variant, of the type's name.
- **A class's shape** has one variant, of the class's name, whose fields are its constructor
  parameters, plain, `val` and `var` alike, in order: what the built-in derivations read. Fields
  its body declares take no part. A repeated parameter is a field of type `Vector[T]`.
- **A shape is made once.** The shapes of a type, its variants and its fields are module values
  private to the type's module, made before `main` like any other; reading one costs a load. A type
  has them where it derives a trait through a template or carries annotations, and no other does;
  every data type derives `Display` and `Debug` through the core library's templates unless it
  implements both itself, so every other data type has its shape.
- The core library declares `TypeShape`, `VariantShape`, `FieldShape`, `Annotation`, `Shaped`, and
  the visitor and builder traits below, in the prelude's module.

## Derive templates

A trait's module says how to derive it with a `derive` declaration: the trait's methods and static
members, written once for a type named after `for`.

```text
derive JsonEncoder for T
    encode(out: JsonWriter) =
        T.walk(self, FieldWriter(out, T.shape()))
        out.endObject()
```

- **One template per trait, in the trait's module**, as a conformance lives in the trait's or the
  type's module. A second template for a trait, or one in another module, is an error. A trait
  without one cannot be derived: `derives X` is then an error saying that X has no derivation. A
  `derives` entry finds the template in its own module, in a module its file imports from, or in
  the core library.
- **The body is an impl's body**: every abstract method and static member of the trait, defaults left
  alone or replaced, each checked against the trait's as an impl's is. `T` stands where an impl names
  its type, and `self` is a `T`. `T` is any name the header gives.
- **`T` conforms to the trait and to the core's `Shaped`**, and to nothing else. `Shaped` gives the
  template the static members below. `Shaped` is named nowhere else: written as a bound anywhere but
  a template, it is an error.
- **The template is checked once**, where it is declared, like any generic code; errors in it are
  the template author's and are reported there, and nowhere else: a type deriving a trait whose
  template has errors reports nothing more.
- **Each deriving type gets the template's body as its impl**, with `T` the type. Its names resolve
  as the template's do, in the trait's module, and each call of a member of `Shaped` on `T` is
  replaced by code written for the type's own variants and fields, which follows.

```text
trait Shaped
    static shape() -> TypeShape
    static variantOf(value: Self) -> VariantShape
    static walk[V: ShapeVisitor](value: Self, visitor: V) -> V
    static walkPair[V: ShapePairVisitor](a: Self, b: Self, visitor: V) -> V
    static build[B: ShapeBuilder[E], E](builder: B) -> Result[Self, E]
```

### Walking a value

`T.walk(value, v)` calls `v.variant` with the value's variant, then `v.field` once per field of that
variant, in order, each call taking the visitor the one before returned:

```text
trait ShapeVisitor
    variant(v: VariantShape) -> Self

// for data Shape = Circle(radius: real) | Rect(width: real, height: real), the compiler writes:
value match
    Circle(r) -> visitor.variant(CIRCLE).field[real](CIRCLE_RADIUS, r)
    Rect(w, h) -> visitor.variant(RECT).field[real](RECT_WIDTH, w).field[real](RECT_HEIGHT, h)
```

**The field hook is not in the trait**, because its bound is the deriving library's own: a visitor
type declares one method `field` with exactly one type parameter of its own, `field[F: Bound](f:
FieldShape, value: F) -> Self`. The hook is resolved for each deriving type, on the visitor type the
template passes to `walk`: the code written there calls it once per field, at the field's type. A
visitor type without such a method, and a field whose type misses any of `Bound`'s traits, are
errors at the `derives` entry, the second naming the field (`Variant.field` in a type of several
variants). A class's fields are read from the object, plain private parameters included.

`T.walkPair(a, b, v)` is the same over two values, for equality and ordering: it calls
`v.variants(T.variantOf(a), T.variantOf(b))`, and, where the two are the same variant, `v.field(f, x,
y)` per field with both values' fields; where they differ, the visitor `variants` returned is the
result. `T.variantOf(x)` is the shape of the variant `x` is, a class's one variant for a class, and
`T.shape()` the type's.

```text
trait ShapePairVisitor
    variants(a: VariantShape, b: VariantShape) -> Self
    // and field[F: Bound](f: FieldShape, a: F, b: F) -> Self
```

### Building a value

`T.build(b)` makes a `T` from what a builder supplies. The builder chooses the variant, then names
the fields **in any order**, so a decoder reads its input as it comes:

```text
trait ShapeBuilder[E]
    variant(t: TypeShape) -> Result[int, E]          // the variant's index
    next(v: VariantShape) -> Result[Option[int], E]  // the next field's index; None when done
    duplicate(f: FieldShape) -> E
    // and field[F: Bound](f: FieldShape) -> Result[F, E]
    //     missing[F: Bound](f: FieldShape) -> Result[F, E]
```

What the compiler writes, for each variant, is equivalent to:

```text
var slot0: Option[F0] = None                   // one slot per field, each at its own type
var slot1: Option[F1] = None
while b.next(VARIANT)? is Some(i)
    i match
        0 -> if slot0.isSome() then return Err(b.duplicate(FIELD_0))
             else slot0 = Some(b.field[F0](FIELD_0)?)
        1 -> …
val x0 = slot0 or else field 0's default or else b.missing[F0](FIELD_0)?
val x1 = …
Ok(Variant(x0, x1))
```

- The builder's `field` and `missing` are its hooks, found and held to the fields as a visitor's
  `field` is; `E` is what the builder's type gives `ShapeBuilder`.
- `variant` is asked even of a type with one variant, so a builder can read what opens a value
  there.
- A field never supplied takes its **default** where its declaration has one, evaluated as the
  constructor would, seeing the fields before it; otherwise the builder's `missing` decides, so a
  library can let an absent `Option` be `None`.
- An index out of range from `variant` or `next` is a bug in the builder, and traps with a message
  naming the type or the variant.
- A class is built through its constructor, a repeated parameter passed as its vector. An abstract
  class cannot be built, nor an open one, whose value could be any subclass: a template that calls
  `build` cannot derive for either, an error at the `derives` entry naming the reason. `walk`,
  `walkPair` and `variantOf` are allowed on both, and a subclass inherits the conformance, so it
  walks as its superclass.

## Annotations

An annotation is a **value of a type implementing `Annotation`**: a data value or an `object`,
written with `@` before a type or a class (on the line before it), or a variant, a field or a class
parameter (on the same line). Its arguments are those of a call of its name, evaluated once, before
`main`, when the shape is made, and stored there. Traits, objects and functions take no annotations
of this kind, nor do a function's parameters.

```text
data JsonKey(name: string)
impl Annotation for JsonKey

object JsonOmit: Annotation

@JsonTag("kind")
data Shape derives JsonEncoder
    @JsonKey("circle") Circle(radius: real)
    Rect(width: real, height: real)

data Person(@JsonKey("full_name") name: string, age: int, @JsonOmit score: int = 0)
```

A template reads them from the shape: `f.annotation[JsonKey]()` is the field's `JsonKey`, if any;
`f.has(JsonOmit)` whether it carries the object. An annotation no template reads is allowed; one
whose name resolves to no type or object, or to one not implementing `Annotation`, is an error. A
misused annotation, such as `@JsonOmit` on a field with no default, is not rejected while compiling:
it fails when a value is decoded. The compiler's own annotations, `@test`, `@tests`, `@intrinsic`
and `@sysl`, stay its own and are not in shapes.

## `derives` with a library trait

`data Order(id: long, lines: List[Line]) derives JsonEncoder` makes the compiler declare, in the
type's module, `impl JsonEncoder for Order` whose body is the template's with `T` as `Order`, and
whose `walk` and `build` are written for `Order`'s shape.

- **Errors name the field.** Where a field's type misses the hook's bound, the error is at the
  `derives` entry: *`Order` cannot derive `JsonEncoder`: field `lines` has type `List[Line]`, which
  does not implement `JsonEncoder`* (here because `Line` does not). Deriving a trait the type also
  implements is an error, a conformance stated twice.
- **Recursion needs nothing.** The derived impl is a conformance, declared before any body is
  checked, so `data Tree(label: string, children: List[Tree]) derives JsonEncoder` holds through
  `List`'s bounded impl and its own; the calls recurse at run time as hand-written ones would.
- **Generic types derive conditionally**, as the derived `Ord`, `Display` and `Debug` do: `data
  Box[A](item: A) derives JsonEncoder` is `impl[A: JsonEncoder] JsonEncoder for Box[A]`, each
  parameter bounded by the trait, so a `Box[int]` encodes and a `Box` of a type without an encoder
  does not. Where the hook's bound is another trait, the type bounds its parameter by it itself,
  `data Labelled[A: FromText](value: A) derives FromPairs`, and the error at the `derives` entry says
  so.
- **Static members are derived too.** A template's `static decode` becomes `Order.decode`, called
  through a bound as `A.decode(r)`.

## Cost

```text
data Person(name: string, age: int) derives JsonEncoder
```

The derived `encode` is `Person.walk(self, FieldWriter(out, PERSON))` then `out.endObject()`, and the
walk is `FieldWriter(…).variant(PERSON_0).field[string](NAME, name).field[int](AGE, age)`.

**What holds now.** Every call is direct: the visitor's type is known and each hook is compiled for
one field type, so there is no call through a method table, no lookup by name and no boxing. A shape
is read with a load of a module value. The visitor costs nothing: `FieldWriter` is data that never
leaves `encode`, so the walk and the hooks are inlined into it and the visitor is kept as its
fields, with no allocation. The compiler's tests hold a derived encoder to exactly this: it
allocates no more than a hand-written one, and calls each field's own method directly.

**What a later compiler feature adds.** Constant data is to be emitted once in read-only data, so
the shapes become constants and code reading them folds: `shape.variantCount > 1` is `false`,
`f.has(JsonOmit)` is `false` and the key is `"name"`, all known to the compiler. With that, what
remains is exactly the hand-written encoder:

```text
encode(out: JsonWriter) =
    out.beginObject()
    out.key("name")
    out.string(name)
    out.key("age")
    out.long(long(age))
    out.endObject()
```

The hooks are inlined at `-O0` too, since the compiler inlines them to keep the visitor as its
fields; there are no lookups by name and no boxing either way. A program
that never encodes a `Person` compiles none of the encoder, though it still makes `Person`'s shape
before `main`.

## A JSON library

The core of a `json` library written with templates. `JsonWriter` writes text and places the
commas; `JsonReader` reads tokens from a byte stream, one at a time.

```text
data PathStep
    Key(name: string)
    Index(at: int)

data JsonError(path: List[PathStep], message: string, line: int, column: int)

impl JsonError
    inField(name: string) -> JsonError = JsonError(Key(name) :: path, message, line, column)
    atIndex(i: int) -> JsonError = JsonError(Index(i) :: path, message, line, column)

trait JsonEncoder
    encode(out: JsonWriter)

trait JsonDecoder
    static decode(r: JsonReader) -> Result[Self, JsonError]
    static whenMissing() -> Option[Self]      // a value for a field absent from the input

data JsonKey(name: string)                    // a field's or variant's name in JSON
impl Annotation for JsonKey
data JsonTag(key: string)                     // a sum type's discriminator key
impl Annotation for JsonTag
object JsonOmit: Annotation                   // never written; read only from its default

keyOf(f: FieldShape) -> string = f.annotation[JsonKey]().map(k -> k.name).getOrElse(f.name)
nameOf(v: VariantShape) -> string = v.annotation[JsonKey]().map(k -> k.name).getOrElse(v.name)
tagOf(t: TypeShape) -> string = t.annotation[JsonTag]().map(t -> t.key).getOrElse("type")
```

Instances for the built-in types and collections are written by hand, the path growing at each
element:

```text
impl JsonEncoder for string
    encode(out: JsonWriter) = out.string(self)

impl JsonDecoder for string
    static decode(r: JsonReader) -> Result[string, JsonError] = r.readString()
    static whenMissing() -> Option[string] = None

impl[A: JsonDecoder] JsonDecoder for Option[A]
    static decode(r: JsonReader) -> Result[Option[A], JsonError] =
        if r.takeNull() then Ok(None) else A.decode(r).map(a -> Some(a))
    static whenMissing() -> Option[Option[A]] = Some(None)

impl[A: JsonDecoder] JsonDecoder for Vector[A]
    static decode(r: JsonReader) -> Result[Vector[A], JsonError] =
        r.beginArray()?
        val items = Buffer[A]()
        while r.nextElement()?
            items.add(A.decode(r).mapErr(e -> e.atIndex(items.length()))?)
        Ok(items.toVector())
    static whenMissing() -> Option[Vector[A]] = None
```

The encoder's derivation. A sum type writes its discriminator first:

```text
derive JsonEncoder for T
    encode(out: JsonWriter) =
        T.walk(self, FieldWriter(out, T.shape()))
        out.endObject()

data FieldWriter(out: JsonWriter, shape: TypeShape)

impl ShapeVisitor for FieldWriter
    variant(v: VariantShape) -> FieldWriter =
        out.beginObject()
        if shape.variantCount > 1
            out.key(tagOf(shape))
            out.string(nameOf(v))
        self

impl FieldWriter
    field[F: JsonEncoder](f: FieldShape, value: F) -> FieldWriter =
        if !f.has(JsonOmit)
            out.key(keyOf(f))
            value.encode(out)
        self
```

The decoder's derivation streams: it reads the discriminator, then each key as it comes, decoding
the value at that field's type, skipping keys it does not know:

```text
derive JsonDecoder for T
    static decode(r: JsonReader) -> Result[T, JsonError] = T.build(FieldReader(r))
    static whenMissing() -> Option[T] = None

data FieldReader(r: JsonReader)

impl ShapeBuilder[JsonError] for FieldReader
    variant(t: TypeShape) -> Result[int, JsonError] =
        r.beginObject()?
        if t.variantCount == 1 then return Ok(0)
        if r.nextKey()? != Some(tagOf(t))
            return Err(r.error(s"expected \"${tagOf(t)}\" first"))
        val name = r.readString().mapErr(e -> e.inField(tagOf(t)))?
        t.variants.find(v -> nameOf(v) == name) match
            Some(v) -> Ok(v.index)
            None -> Err(r.error(s"unknown variant \"$name\"").inField(tagOf(t)))

    next(v: VariantShape) -> Result[Option[int], JsonError] =
        r.nextKey()? match
            None -> Ok(None)
            Some(key) -> v.fields.find(f -> !f.has(JsonOmit) && keyOf(f) == key) match
                Some(f) -> Ok(Some(f.index))
                None ->
                    r.skipValue()?
                    next(v)

    duplicate(f: FieldShape) -> JsonError = r.error(s"duplicate field \"${keyOf(f)}\"")

impl FieldReader
    field[F: JsonDecoder](f: FieldShape) -> Result[F, JsonError] =
        F.decode(r).mapErr(e -> e.inField(keyOf(f)))

    missing[F: JsonDecoder](f: FieldShape) -> Result[F, JsonError] =
        F.whenMissing() match
            Some(v) -> Ok(v)
            None -> Err(r.error(s"missing field \"${keyOf(f)}\""))
```

Using it:

```text
data Address(street: string, city: string) derives JsonEncoder, JsonDecoder

data Person(@JsonKey("full_name") name: string, age: int, home: Option[Address],
            @JsonOmit visits: int = 0) derives JsonEncoder, JsonDecoder

@JsonTag("kind")
data Shape derives JsonEncoder, JsonDecoder
    @JsonKey("circle") Circle(radius: real)
    @JsonKey("rect") Rect(width: real, height: real)
    Empty
```

`Person("Ada", 36, None)` encodes as `{"full_name":"Ada","age":36,"home":null}`; decoding that, or
the same without `"home"`, gives it back with `visits` at `0`. `Rect(2.0, 3.0)` is
`{"kind":"rect","width":2.0,"height":3.0}`. Decoding `{"full_name":"Ada","age":36,"home":{"street":
1}}` fails with the path `home.street` and the position of the `1`. A discriminator that is not the
object's first key is refused by this decoder; a library that wants to accept it reads that object
into a tree first and decodes from the tree, with nothing new from the language.

## The built-in derivations

The five traits the core library derives are templates in the same mould, each a few lines in the
trait's own file of the core library, compiled for each deriving type as any library's are:

```text
derive Hash for T
    hash() -> long = T.walk(self, hashFrom).h

private data HashFields(h: long)
private val hashFrom = HashFields(0)

impl ShapeVisitor for HashFields
    variant(v: VariantShape) -> HashFields = if v.index == 0 then self else HashFields(long(v.index))

impl HashFields
    field[F: Hash](f: FieldShape, value: F) -> HashFields = HashFields(h *% 31 +% value.hash())

derive Ord for T
    compare(other: T) -> int = T.walkPair(self, other, orderedSoFar).result

private data OrderFields(result: int)
private val orderedSoFar = OrderFields(0)
private val orderedBefore = OrderFields(-1)
private val orderedAfter = OrderFields(1)

impl ShapePairVisitor for OrderFields
    variants(a: VariantShape, b: VariantShape) -> OrderFields =
        if a.index == b.index then self elif a.index < b.index then orderedBefore else orderedAfter

impl OrderFields
    field[F: Ord](f: FieldShape, a: F, b: F) -> OrderFields =
        if result != 0 then return self
        val c = a.compare(b)
        if c == 0 then self elif c < 0 then orderedBefore else orderedAfter

derive Display for T
    display(out: Writer, spec: FormatSpec)
        if spec.width > 0
            val text = StringWriter()
            display(text, FormatSpec())
            out.writeField(text.text(), spec)
        else
            T.walk(self, DisplayFields(out))
            if T.variantOf(self).fieldCount > 0 then out.write(")")

private data DisplayFields(out: Writer)

impl ShapeVisitor for DisplayFields
    variant(v: VariantShape) -> DisplayFields =
        out.write(v.name)
        self

impl DisplayFields
    field[F: Display](f: FieldShape, value: F) -> DisplayFields =
        out.write(if f.index == 0 then "(" else ", ")
        value.display(out, FormatSpec())
        self
```

`Eq` is `Ord`'s shape with `equals`, and `Debug` is `Display`'s with `debug`. The visitors of `Eq`
and `Ord` take only a few values, each a module value, so comparing allocates nothing, and a derived
`compare` is -1, 0 or 1; a derived `Display` allocates its one visitor, and a `Hash` one per field. A class without
constructor parameters has a variant without fields, so it displays as its bare name, as such a
variant does.

What stays the compiler's is when the templates apply. A class derives `Eq` and `Hash` together,
and `Ord` only beside an `Eq` by the same fields; a data type's `==` and hash stay the language's
own, by value, as they are for tuples, so `derives Eq, Hash` there adds nothing; and the automatic
`Display` and `Debug` of every data type are these templates applied without asking, where a field
missing the hook's bound leaves the type without the trait instead of being an error. A field
missing `Ord`, `Display` or `Debug` where the type derives it is reported before any body is
checked: *`Ranked` cannot derive `Ord`: its field `plain` has type `Plain`, which does not implement
`Ord`*.

## What this does not do

- **No code is generated from a library's text.** A template is ordinary Avin, checked once; the
  compiler writes only the walk and the build, whose shape it already knows.
- **No reading of types at run time.** A shape is constant data about one type; nothing looks a type
  up by name or enumerates the program's types.
- **No compile-time evaluation of library code.** A template cannot reject a misuse of its
  annotations while compiling, such as `@JsonOmit` on a field with no default; that failure shows
  when a value is decoded.
