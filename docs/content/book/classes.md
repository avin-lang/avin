---
title: Classes and traits
summary: Objects with state, shared behaviour through traits, inheritance, and implementations for types you did not write.
weight: 8
---

A data value is a value: it never changes, and two with the same fields are equal. Some things in a
program are not like that. A bank account has a balance that goes up and down, and two accounts with
the same balance are still two accounts. A **class** describes things like these: objects with
identity and, where they need it, state that changes.

```text run
class Account(val owner: string, var balance: int = 0)
  deposit(amount: int)
    require(amount > 0, "a deposit is more than nothing")
    balance += amount

  withdraw(amount: int) -> bool
    if amount > balance then return false
    balance -= amount
    true

main()
  val account = Account("Ada")
  account.deposit(100)
  println(account.withdraw(30))
  println(account.withdraw(500))
  println(s"${account.owner} has ${account.balance}")
```

```text output
true
false
Ada has 70
```

The class's parameters, `(val owner: string, var balance: int = 0)`, are what `Account(…)` is given
to make an object. `val` makes a parameter a field others can read, and `var` one they can read and
the class can change. The indented lines are the class's **methods**, functions that belong to each
object and read its fields by name. `require` stops the program when a caller breaks a rule the
method depends on, with the message given. The [class](/spec/declarations/#class) section of the
specification has the details.

A method with no parameter list is read like a field, `t.fahrenheit`, and is written that way when it
only computes a value from the object, with nothing to do and nothing to change:

```text run
class Temperature(celsius: real)
  fahrenheit -> real = celsius * 9.0 / 5.0 + 32.0
  isFreezing -> bool = celsius <= 0.0

main()
  val t = Temperature(100.0)
  println(t.fahrenheit)
  println(t.isFreezing)
  println(Temperature(-5.0).isFreezing)
```

```text output
212
false
true
```

## Traits

A **trait** names what several types have in common: methods each one must have, and methods
written once for all of them in terms of the others. A class that extends a trait writes the methods
the trait leaves open, marking each with `override`.

```text run
trait Shape
  area() -> real
  name() -> string
  describe() -> string = s"a ${name()} of area ${area()}"

class Square(side: real): Shape
  override area() -> real = side * side
  override name() -> string = "square"

class Circle(radius: real): Shape
  override area() -> real = 3.0 * radius * radius
  override name() -> string = "circle"
  override describe() -> string = s"a round ${name()}, about ${area()} across"

main()
  val shapes: Vector[Shape] = [Square(2.0), Circle(1.0), Square(1.5)]
  for s in shapes do println(s.describe())
  println(shapes.map(_.area()).sum)
```

```text output
a square of area 4
a round circle, about 3 across
a square of area 2.25
9.25
```

`describe` has a body in the trait, so `Square` gets it as it is, and `Circle` replaces it with its
own. A `Vector[Shape]` holds squares and circles alike, and each call of `describe` runs the method of
the object it is called on. The [trait](/spec/declarations/#trait) section has more.

## Inheritance

A class is closed to extension unless it says otherwise. An `abstract` class leaves some methods for
its subclasses to write and cannot itself be made, and an `open` class can be extended as it is. A
subclass names its superclass after `:`, with the arguments for the superclass's parameters.

```text run
abstract class Animal(val name: string)
  sound() -> string
  speak() -> string = s"$name says ${sound()}"

class Dog(name: string): Animal(name)
  override sound() -> string = "woof"

class Cat(name: string): Animal(name)
  override sound() -> string = "meow"
  override speak() -> string = s"$name ignores you"

main()
  val animals: Vector[Animal] = [Dog("Rex"), Cat("Tom"), Dog("Fido")]
  for a in animals do println(a.speak())
  for a in animals
    a match
      d: Dog -> println(s"${d.name} is a dog")
      c: Cat -> println(s"${c.name} is a cat")
      _ -> println("something else")
```

```text output
Rex says woof
Tom ignores you
Fido says woof
Rex is a dog
Tom is a cat
Fido is a dog
```

`d: Dog` is a **type pattern**: it matches an `Animal` that is a `Dog`, and names it `d` as a `Dog`.
It is how Avin asks what class an object really is.

## Implementations for any type

`impl` adds a trait to a type after the type is written, even a type from somewhere else. It is how a
type gets the library's traits, as `ParseError` got `Display` in [Data](/book/data/), and how your
own traits reach types you did not write:

```text run
trait Describe
  describe() -> string

impl Describe for int
  describe() -> string = if self % 2 == 0 then s"the even number $self" else s"the odd number $self"

impl Describe for string
  describe() -> string = s"the text \"$self\", ${self.charCount} characters long"

data Point(x: int, y: int)

impl Describe for Point
  describe() -> string = s"the point at $x across and $y up"

main()
  println(7.describe())
  println("hello".describe())
  println(Point(2, 3).describe())
```

```text output
the odd number 7
the text "hello", 5 characters long
the point at 2 across and 3 up
```

`self` is the value the method was called on. A class says how it is written as text the same way,
through `impl Display`:

```text run
class Money(cents: long)
  plus(other: Money) -> Money = Money(cents + other.cents)

impl Display for Money
  display(out: Writer, spec: FormatSpec) = out.write(f"$$${cents / 100}.${cents % 100}%02d")

main()
  val price = Money(1999)
  val tax = Money(160)
  println(price)
  println(s"total: ${price.plus(tax)}")
```

```text output
$19.99
total: $21.59
```

See [impl](/spec/declarations/#impl) in the specification.

## Objects

`object` declares a class with exactly one instance, made when it is first used. It suits something a
program has only one of, such as a registry:

```text run
object Registry
  var names = Vector.empty[string]()
  add(name: string) = names = names :+ name

main()
  Registry.add("first")
  Registry.add("second")
  println(Registry.names)
```

```text output
[first, second]
```

## Exercises

1. Give `Account` a method `transfer(to: Account, amount: int) -> bool` that moves money between two
   accounts only when there is enough.
2. Add a `Triangle` class to the `Shape` trait, and a method `perimeter()` with a default that traps
   with `todo()` until each class writes its own.
3. Write a trait `Scalable` with `scaled(factor: real) -> Self`, and implement it for `Square`.
