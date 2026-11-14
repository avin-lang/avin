---
title: Refined variants
summary: Data variants that fix their type's arguments, and match arms that learn those arguments, so a typed interpreter, a typed protocol or a typed plan of transitions checks at compile time.
weight: 14
---

A variant of a generic `data` type may say which instance of the type it makes. A `match` arm that
takes such a variant apart then knows the instance, and the code in the arm is checked with that
knowledge:

```text
data Expr[T]
  IntLit(value: int) -> Expr[int]
  BoolLit(value: bool) -> Expr[bool]
  Add(a: Expr[int], b: Expr[int]) -> Expr[int]
  Eq(a: Expr[int], b: Expr[int]) -> Expr[bool]
  If(cond: Expr[bool], yes: Expr[T], no: Expr[T])
  Pair[A, B](first: Expr[A], second: Expr[B]) -> Expr[(A, B)]
  First[A, B](pair: Expr[(A, B)]) -> Expr[A]

eval[T](e: Expr[T]) -> T
eval(IntLit(n)) = n
eval(BoolLit(b)) = b
eval(Add(a, b)) = eval(a) + eval(b)
eval(Eq(a, b)) = eval(a) == eval(b)
eval(If(c, yes, no)) = if eval(c) then eval(yes) else eval(no)
eval(Pair(a, b)) = (eval(a), eval(b))
eval(First(p)) =
  val (a, _) = eval(p)
  a

main()
  val e = If(Eq(IntLit(2), Add(IntLit(1), IntLit(1))), Pair(IntLit(7), BoolLit(true)), Pair(IntLit(0), BoolLit(false)))
  println(eval(e)) // (7, true)
  println(eval(First(e)) + 1) // 8
```

`IntLit(n)` is an `Expr[int]`, so in the clause that takes it apart `T` is `int`, `n` is a `T`
there, and `n` is a well-typed result. `Add(IntLit(1), BoolLit(true))` is a type error where it is
written, so `eval` never meets one. Nothing is tested at run time beyond the variant itself.

A variant written as before, `If(…)`, makes the type at its own parameters, `Expr[T]`, as every
variant of every `data` type does; a type none of whose variants fixes anything is an ordinary
`data` type, and everything in [data](/spec/declarations/#data) holds of these types unchanged.

## Declaring a refined variant

A variant's **result** follows its fields after `->`: `IntLit(value: int) -> Expr[int]`, and with no
fields, `Balance -> Query[long]`. A variant may declare **type parameters of its own** after its
name, `Pair[A, B](…)`, with bounds as any type parameter takes (`Ask[R: Sendable](…)`). In a variant,
the data type's own parameter names are the variant's parameters too, so `If`'s `T`, and `T` in a
result `-> Expr[T]`, need no declaring; a variant parameter may not reuse one of those names.

The declaration is checked as follows.

1. **The result is an instance of the type being declared.** `IntLit(value: int) -> Option[int]` in
   `data Expr[T]` is refused. Its arguments are any types built from the variant's parameters and
   the program's types: `Expr[int]`, `Expr[(A, B)]`, `Expr[Vector[A]]`, `Expr[(A) -> B]`.
2. **A parameter declared with a variance is never fixed.** Where the type's parameter is `+T` or
   `-T`, the result's argument in that place is `T` itself. A variant fixes only invariant
   parameters ([Variance](#variance)).
3. **A union in a result's argument names no variant parameter.** `-> Expr[int | string]` is
   allowed; `-> Expr[A | string]` is refused, since a value of `Expr[int | string]` would not say
   which part `A` is.
4. **Every declared variant parameter is used**, in a field or in the result. A parameter named in
   the fields and not in the result is a [hidden parameter](#hidden-type-parameters).
5. **The field types are checked for variance against the data type's parameters**, as any
   variant's are, for each parameter the variant leaves unfixed. A fixed or hidden parameter has no
   variance: the variant's fields are read only through a `match` that knows them exactly.
6. **A field does not name the type at a larger instance than the variant makes**, unless the larger
   part comes from a hidden parameter. `Lift[A](x: Expr[Vector[A]]) -> Expr[A]` is refused as a
   [nested data type](/spec/generics/#polymorphic-recursion) is: `Expr[int]` would hold an
   `Expr[Vector[int]]`, which would hold an `Expr[Vector[Vector[int]]]`, and every instance needs its
   own compiled copy. `First[A, B](pair: Expr[(A, B)]) -> Expr[A]` is allowed: its larger part
   holds `B`, which a value has only where the program made one
   ([Compilation](#compilation)).

### Why it is sound

A variant is a constructor, and its result is the constructor's signature: `Pair` is a generic
function `[A, B](first: Expr[A], second: Expr[B]) -> Expr[(A, B)]`. A value can only come from a
call of it, so its type at run time is the result at the arguments the call chose, and the header of
the object says so, as every object's [header names its exact type](/spec/generics/#reification).
Rules 1 and 3 make that type something a `match` can read back: every parameter in the result sits
under constructors that the type's name and arguments determine (a tuple, a generic type, a function
type), so a known instance gives exactly one answer for each. Rule 2 keeps an instance's arguments
exact where the result fixes them. Rules 5 and 6 are the checks every data type already passes,
stated for a variant whose result is not the plain instance.

## Making a value

A variant is called as a generic function is, its type arguments inferred from its fields and from
the type expected, or written: `Pair[int, bool](IntLit(1), BoolLit(true))`, and for a variant of
the data type's own parameters, on the type as before (`Expr[int].If(…)`). The value has the result's
type at those arguments.

```text
val ok: Expr[bool] = Eq(IntLit(1), IntLit(2))
val bad: Expr[bool] = IntLit(1)
// error: `IntLit(1)` is an `Expr[int]`, and an `Expr[bool]` is expected here
val worse = Add(IntLit(1), BoolLit(true))
// error: `BoolLit(true)` is an `Expr[bool]`, and the field `b` of `Add` is an `Expr[int]`
```

A bare variant name is decided by the type expected, as [any shared name is](/spec/declarations/#data);
a refined variant whose result cannot be the expected instance is reported as above.

## Refinement in a match

Matching a value of type `D[X1, …, Xn]` against a variant whose result is `D[R1, …, Rn]` gives the
arm **local type equalities**: for each fixed place, `Xi = Ri`, with the variant's own parameters
standing for fresh types known only by these equalities. The equalities are solved at once, by
taking both sides apart: `(A, B) = (int, bool)` is `A = int` and `B = bool`, `Vector[A] = Vector[T]`
is `A = T`, and two different type constructors or two different types (`int = bool`,
`Vector[A] = Option[T]`) are a contradiction. A type parameter may be equated with a type that does
not contain it; `T = Vector[T]` is a contradiction too.

The solution is a **most general substitution** of the type parameters in scope, the enclosing
function's and class's and the variant's own, and it holds in the arm:

- in the arm's guard and body, and in the patterns inside the variant's, which may refine further:
  `Add(IntLit(n), b)` learns nothing more, and `If(BoolLit(b), x, y)` learns `T`'s equalities from
  `If` only;
- in a function's [clause](/spec/declarations/#clauses), for the clause's guard and body, each
  argument's pattern contributing;
- for [`is`](/spec/operators-and-control-flow/#is), exactly where the names the pattern binds are in
  scope: the `then` branch of `if e is IntLit(n)`, the rest of the block after
  `if e is not IntLit(n) then return …`, the right operand of `&&`;
- for a destructuring binding, `val Refl = proof`, in the rest of the block, the binding's pattern
  covering every value of its type as it must;
- in a [match function](/spec/operators-and-control-flow/#match-functions) and in an actor's or a
  machine's arms, as in any `match`.

Inside the arm a type and anything it equals are one type: `n: int` is a `T`, a `T` takes `int`'s
operators and methods, and a `Vector[A]` is a `Vector[int]`. Outside it nothing is known:

- **An arm's result is checked against the type expected of the `match`** under the arm's
  equalities: in `eval`, each clause's body is checked against `T`. Where no type is expected, an
  arm yields its own type as written, without equalities, so arms yielding `int` and `T` agree on no
  type and the result is to be declared. A non-recursive function whose result is inferred from
  such a `match` is reported with the suggestion to declare it.
- **An inference variable settled inside an arm** settles to the type as written there: `best =
  Some(n)` with `n: int` makes `best` an `Option[int]`, and not an `Option[T]`, so a store of a `T`
  elsewhere is an error there.
- **A variable declared outside the arm keeps its declared type**, and the arm may store into it any
  value of that type under the equalities: `var out: T` takes `n` in the `IntLit` arm.

An arm learns equalities from variants only. An extractor pattern does not refine, since an
`unapply`'s result type is its own signature's, and neither does a type pattern: `x: Expr[int]` on
an `Any` binds an `Expr[int]` and learns nothing of any type parameter.

### Why it is sound

Avin compiles a copy of each generic function for each list of type arguments the program uses, so
in any copy that runs, `T` is one particular type, the same for the whole call. A value reaching the
arm was made by the variant at some arguments, its type is the variant's result at them, and that
type is the value matched's type at every fixed place ([Variance](#variance)). So the equalities hold
of the copy's types, every time the arm runs, and the most general substitution is implied by them:
whatever the body is checked to do under the substitution, it does in that copy. They are facts
about types, which never change while the program runs, so a lambda made in the arm and called later
still sees them true, and a variable stored in the arm holds a value of its declared type.

A contradiction means no copy and no value can reach the arm, which is why such an arm is an error
([Coverage](#coverage)). Settling inference variables as written, and checking each arm's result
against the expected type rather than letting its type out, keep every equality inside the code it
is true for: a type that leaves the arm is one the code outside can rely on without the arm's facts.

## Variance

A type parameter declared `+T` or `-T` cannot be fixed by a variant, so a covariant or a
contravariant data type is refined only in its invariant places. The reason is what a value's static
type promises. An instance's invariant argument is exact: a value of `Expr[T]` was made as an
`Expr[T]`, since an invariant instance is an instance only of itself. A covariant argument is an
upper bound: a value of `Box[Animal]` may have been made as a `Box[Dog]`. An equality learned from a
covariant place would be false. With

```text
data Box[+T]
  Holds(value: T)
  DogBox(dog: Dog) -> Box[Dog] // error: `Box` is covariant in `T`, so a variant cannot fix `T`:
  // `DogBox` makes a `Box[Dog]`; declare the parameter invariant, `data Box[T]`
```

a function `asDog[T](b: Box[T], x: T) -> Dog = b match DogBox(_) -> x` would be checked under
`T = Dog`, and `asDog(DogBox(rex), cat)` is a legal call at `T = Animal` (a `Box[Dog]` is a
`Box[Animal]`, a cat is an `Animal`), which would answer the cat as a `Dog`. What the arm truly
learns there is `Dog` is a subtype of `T`, a bound in one direction; Avin does not refine by bounds,
and keeps the rule a single one: a variant fixes invariant parameters, and the parameters it leaves
alone keep their variance, checked as before.

### Why it is sound

Invariant subtyping is equality, and type equality in Avin is identity once aliases are expanded and
unions are in [normal form](/spec/unions/#normal-form). So at a fixed place the static argument and
the argument the value was made with are the same type, which is the premise of the equalities.
The places a variant leaves unfixed take the data type's own parameter, so a value seen through a
wider instance there is read through fields whose variance the declaration already checked.

## Hidden type parameters

A variant parameter that its fields name and its result does not is **hidden**: the instance does not
say what it is, so each value carries its own. A typed protocol needs one, since a request and the
address of its reply must agree on a type that the message type does not mention:

```text
import avin.actors.{ActorRef, AskError, spawn}
import avin.threads.Future
import avin.time

data Query[R]
  Balance -> Query[long]
  Owner -> Query[string]
  Deposit(amount: long) -> Query[unit]

data AccountMsg
  Ask[R: Sendable](query: Query[R], replyTo: ActorRef[R])

actor Account(owner: string) receives AccountMsg
  var balance = long(0)

  answer[R](q: Query[R]) -> R
  answer(Balance) = balance
  answer(Owner) = owner
  answer(Deposit(amount)) = balance += amount

  receive
    Ask(query, replyTo) -> replyTo.tell(answer(query))

request[R: Sendable](account: ActorRef[AccountMsg], q: Query[R]) -> Future[Result[R, AskError]] =
  account.ask(r -> Ask(q, r), timeout = time.seconds(1))

main()
  val account = spawn(Account("ada"))
  request(account, Deposit(40)).await()
  println(request(account, Balance).await()) // Ok(40)
  println(request(account, Owner).await()) // Ok(ada)
```

`request(account, Balance)` is a `Future[Result[long, AskError]]`, and a reply of any other type is
an error in `Account`, not a surprise in its caller: in the `Ask` arm `R` is a fresh type, the same
for `query` and `replyTo`, so `replyTo.tell(answer(query))` is well typed and `replyTo.tell(balance)`
is not (`` `balance` is a `long`, and `replyTo` takes an `R` ``).

- **In the arm a hidden parameter is a fresh type**, with the bounds its declaration gives it and no
  other knowledge, distinct from every other type; nested patterns may refine it, as `answer`'s
  clauses refine `R`.
- **A hidden parameter never leaves its arm.** The arm's result, checked against the type expected,
  must not need it; with no expected type, an arm whose type names it is an error. A local declared
  outside the arm cannot be settled to a type naming it.
- **Making a value** chooses the hidden parameter at the call, inferred or written
  (`Ask[long](Balance, r)`).

### Why it is sound

The value was made by a call at one type for `R`, so its `query` and its `replyTo` agree on it, and
that is all the arm assumes. A fresh type equal to no other cannot be confused with a type outside
the arm, and since the arm's result cannot name it, no value of it is seen as anything else after
the arm. Each value carries its own, so two values matched one after the other give two unrelated
fresh types, and a program cannot pass one's query to the other's reply.

## Bounds on a variant's parameters

A variant parameter's bounds are checked where a value is made, as a call's are: `Ask` with a reply
type that is not `Sendable` is an error at the call. In the arm the parameter has its bounds, so
`Sendable` holds of `R` and `ActorRef[R]` is a type there. Where a fixed parameter is equated with a
type parameter of the enclosing code, that type gains the bound in the arm: with
`Sorted[A: Ord](items: Vector[A]) -> Coll[A]`, a function `smallest[T](c: Coll[T])` may call
`c.items.min` in the `Sorted` arm, where `T` is `Ord`.

### Why it is sound

No value of the variant exists at arguments that break its bounds, so wherever the arm runs, the
arguments meet them. A copy at a type without the bound reaches the arm with no value, so the arm is
not compiled there ([Compilation](#compilation)) and the bound is never needed of that type.

## Coverage

A `match` is checked for coverage over the variants **possible at the type matched**: those whose
result's equalities with that instance have a solution. On an `Expr[int]` that is `IntLit`, `Add`,
`If` and `First`, so

```text
total(e: Expr[int]) -> int = e match
  IntLit(n) -> n
  Add(a, b) -> total(a) + total(b)
  If(c, a, b) -> if eval(c) then total(a) else total(b)
  First(p) ->
    val (n, _) = eval(p)
    n
```

covers every value with no arm for `BoolLit`, `Eq` or `Pair`. In generic code each type parameter is
a type of its own, so on an `Expr[T]` every variant is possible, and on an `Expr[(A, B)]` every
variant but `IntLit`, `BoolLit`, `Add` and `Eq`.

- **The equalities of a row travel along it.** Matching a tuple of two `Expr[T]`s,
  `(IntLit(_), BoolLit(_))` is impossible, since the first part needs `T = int` and the second
  `T = bool`; the arms need not cover it, and an arm written for it is an error.
- **The equalities of an enclosing arm hold in a nested `match`**: inside `eval`'s `IntLit` clause a
  `match` on another `Expr[T]` is a match on an `Expr[int]`.
- **An arm no value can reach because its equalities contradict is an error**, as every unreachable
  arm is, and the message says which equality fails:

  ```text
  isZero(e: Expr[int]) -> bool = e match
    IntLit(n) -> n == 0
    Eq(_, _) -> false
    // error: this arm is never reached: `Eq` makes an `Expr[bool]`, and the value matched is an
    // `Expr[int]`
    _ -> false
  ```

- **An instance at which no variant is possible has no values.** With
  `data Same[A, B]` and its one variant `Refl -> Same[A, A]`, a `Same[int, string]` cannot be made,
  and a `match` on one is an error (`` `Same[int, string]` has no values: no variant of `Same`
  makes one ``).
- Guards, extractors and the rest count as they do in [match](/spec/operators-and-control-flow/#match).
  The value a missing case is named by is one of a possible variant.

### Why it is sound

A variant left out is one no value of the matched type can be: a value of `Expr[int]` was made by a
variant whose result is `Expr[int]` at some arguments, and one whose result cannot be is not it.
Along a row, all the row's patterns match the same value at the same types, so their equalities hold
together or the row matches nothing. Pruning removes only cases that have no values, so a `match`
judged to cover its type covers every value that reaches it.

## Compilation

Refinement costs nothing at run time. Generics are [monomorphized](/spec/generics/#reification):
each copy of a generic function has concrete type arguments, and at them every equality is decided
when the copy is made.

- **An arm whose equalities fail at the copy is not compiled**: no test, no body. In `eval[bool]`
  the `IntLit` clause is gone, and in `eval[int]` the `IntLit` clause's body is `n`, an `int`
  answered as the `int` it is. The tests that remain are the variant tests every `match` makes.
- **An instance's layout holds the variants possible at it.** `Expr[bool]` has descriptors for
  `BoolLit`, `Eq`, `If` and `First`, none for `IntLit`. A variant keeps its declared position as its
  tag at every instance, so equality, hashing and the derived `Display` and `Debug` are compiled per
  instance over the variants it has.
- **A variant with hidden parameters has a descriptor for each list of them the program makes it
  with** (a **pack**): `Ask[long]`, `Ask[string]` and `Ask[unit]` above, all of tag 0. The program is
  compiled whole, so the packs are known, as the types implementing a trait are for
  [generic trait methods](/spec/generics/#reification). A field of a hidden type is laid out at the
  pack's type, unboxed where it is a value type. An arm on such a variant is compiled once per pack
  reaching it, as a function of the hidden types, and the `match` calls the one the value's
  descriptor names. Equality compares the descriptors before the fields: two values of one variant
  in different packs are unequal.
- **A cycle that makes packs from ever larger hidden types** is
  [polymorphic recursion](/spec/generics/#polymorphic-recursion) and is reported as one; recursion
  through an arm that passes a part of the value matched, as `eval(a)` passes `Expr[A]` taken from
  an `Expr[(A, B)]`, shrinks and is ordinary.

### Why it is sound

The checker accepted the body under the most general substitution; a copy is that body at one
instance of it, so it is well typed at the copy whenever the copy's arguments satisfy the
equalities, and when they do not, no value reaches the arm and leaving it out changes nothing. A
pack's descriptor records the hidden types the value was made with, so the arm compiled for that
pack is the one whose types are the value's. The descriptors and packs are finite because the
copies are, which the polymorphic-recursion check guarantees.

## A typed plan of transitions

A [state machine](/spec/actors/#state-machines) accepts any event in any state and handles it there.
Where the order of events is itself the contract, a refined type states it, and a plan that breaks
it is a type error before the machine sees an event:

```text
data DoorShut()
data DoorOpen()
data DoorLocked()

data Step[From, To]
  OpenIt -> Step[DoorShut, DoorOpen]
  CloseIt -> Step[DoorOpen, DoorShut]
  LockIt(code: int) -> Step[DoorShut, DoorLocked]
  UnlockIt(code: int) -> Step[DoorLocked, DoorShut]

data Plan[From, To]
  Done -> Plan[From, From]
  Then[Mid](step: Step[From, Mid], rest: Plan[Mid, To])

event[From, To](s: Step[From, To]) -> DoorEvent
event(OpenIt) = Open
event(CloseIt) = Close
event(LockIt(code)) = Lock(code)
event(UnlockIt(code)) = Unlock(code)

perform[From, To](door: Door, plan: Plan[From, To]) -> unit
perform(door, Done) = ()
perform(door, Then(step, rest)) =
  door.handle(event(step))
  perform(door, rest)

main()
  val night: Plan[DoorShut, DoorShut] = Then(LockIt(42), Then(UnlockIt(42), Done))
  perform(Door(), night)
  val wrong: Plan[DoorShut, DoorShut] = Then(OpenIt, Then(LockIt(1), Done))
  // error: `LockIt(1)` is a `Step[DoorShut, DoorLocked]`, and a `Step[DoorOpen, _]` is expected
  // here
```

`Done`'s result, `Plan[From, From]`, fixes `To` to `From`, so a plan ends where it began only through
the steps between; `Then`'s `Mid` is hidden, chained from one step to the next. `perform` recurses at
`Plan[Mid, To]`, a pack each, with one copy per state type the program's plans pass through.

The same shape gives a type equality a value can carry: `data Same[A, B]` with `Refl -> Same[A, A]`,
and a function that turns an `A` into a `B` given a proof:

```text
cast[A, B](proof: Same[A, B], x: A) -> B
cast(Refl, x) = x
```

well typed because the clause learns `A = B`.

## Diagnostics

| Where | Message |
| --- | --- |
| A result of another type | `` the result of a variant is an instance of its own type: `IntLit` of `Expr` declares `Option[int]` `` |
| A fixed variance parameter | `` `Box` is covariant in `T`, so a variant cannot fix `T`: `DogBox` makes a `Box[Dog]`; declare the parameter invariant, `data Box[T]` `` |
| A union naming a parameter | `` `A` is inside a union in the result of `V`, so a value of the type does not say what `A` is `` |
| An unused variant parameter | `` `A` is declared by `V` but named by none of its fields and not by its result `` |
| A reused name | `` `T` is already a parameter of `Expr`, and stands for the same type in each of its variants `` |
| A larger instance | the [nested data type](/spec/generics/#polymorphic-recursion) error, naming the field and the variant's result |
| Making a value | `` `IntLit(1)` is an `Expr[int]`, and an `Expr[bool]` is expected here `` |
| An impossible arm | `` this arm is never reached: `Eq` makes an `Expr[bool]`, and the value matched is an `Expr[int]` `` |
| A contradiction in a row | `` this arm is never reached: `IntLit` needs `T` to be `int`, and `BoolLit` in the same pattern needs it to be `bool` `` |
| A cyclic equality | `` this arm is never reached: `Wrap` needs `T` to be `Vector[T]`, which no type is `` |
| An empty instance | `` `Same[int, string]` has no values: no variant of `Same` makes one `` |
| An unknown type matched | `` the type of the value matched must be known to match `IntLit`, which fixes the type's argument; write the type where the value is declared `` |
| A hidden type leaving its arm | `` this arm's result is an `R`, a type `Ask` hides in the value matched, which is not known outside the arm `` |
| Arms needing an expected type | `` the arms of this `match` agree on no type: `IntLit`'s yields an `int`, which is `T` only inside that arm; declare the result type `` |
| A missing case | the usual message, naming a possible variant: `` this `match` does not cover every Expr[int] value, such as `If(_, _, _)` `` |
| Growing packs | the [polymorphic recursion](/spec/generics/#polymorphic-recursion) error, naming the variant whose hidden type grows |
