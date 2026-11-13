---
title: Dates and times
summary: "`avin.time`: eight types that keep the timeline, the calendar and the zone apart — `Instant`, `Duration`, `Date`, `Time`, `DateTime`, `Zone`, `Zoned` and `Period` — the two clocks, arithmetic across month ends and daylight saving, ISO 8601 text and patterns."
weight: 23
---

`avin.time` is the standard library's module for clocks, lengths of time, the calendar and time
zones. It is written in Avin; the clocks, the host's zone database and the local zone are reached
through [Sysl bindings](/spec/sysl-bindings/), and the calendar arithmetic is Sysl's `sysl.time`.

```text
import avin.time.{now, monotonic, date, dateTime, zone, hours, months}

main()
  val start = monotonic()
  val toronto = zone("America/Toronto")?
  val meeting = dateTime(2026, 3, 7, 9, 0).atZone(toronto)?
  println(meeting.plusDays(1)) // 2026-03-08T09:00:00-04:00[America/Toronto]
  println(meeting + hours(24)) // 2026-03-08T10:00:00-04:00[America/Toronto]
  println(date(2026, 1, 31) + months(1)) // 2026-02-28
  println(s"stamped ${now()}, took ${monotonic() - start}")
```

## Eight types

**Each type answers one kind of question, so a question a value cannot answer does not compile.**
An instant has no year until a zone is named; a date has no hour; a time of day has no date.

| Type | What it is | Representation | Made by |
| --- | --- | --- | --- |
| `Instant` | a point on the timeline, with no calendar | `data Instant(epochMicros: long)`, microseconds since 1970-01-01T00:00:00Z | `now()`, `epochSeconds(n)`, `epochMillis(n)`, `epochMicros(n)` |
| `Duration` | an exact length of timeline, negative, zero or positive | `data Duration(micros: long)` | `micros(n)` … `weeks(n)`, `2.hours`; `monotonic()` |
| `Date` | a calendar date, with no time of day and no zone | `data Date(epochDay: int)`, days since 1970-01-01 | `date(y, m, d)` |
| `Time` | a time of day, with no date and no zone | `data Time(microsOfDay: long)`, `0` to `86_399_999_999` | `time(h, m)`, `time(h, m, s)`, `time(h, m, s, us)` |
| `DateTime` | a wall reading, a date and a time of day, with no zone | `data DateTime(date: Date, time: Time)` | `dateTime(…)`, `d.atTime(t)` |
| `Zone` | a zone: a name and the offset its clocks show at any instant | a name and its rules: one fixed offset, or the bytes of the host database's file for it, which Sysl's `sysl.time.tzif` decodes | `zone(name)`, `localZone()`, `utc`, `fixedZone(offset)` |
| `Zoned` | a wall reading in a zone; the only zoned type | `data Zoned(dateTime: DateTime, offset: Duration, zone: Zone)` | `t.atZone(z)`, `dt.atZone(z)` |
| `Period` | a length of calendar: months and days | `data Period(months: int, days: int)` | `months(n)`, `years(n)`, `Period(months, days)` |

Beside them: `Weekday`, a `data` type of the seven days `Monday` to `Sunday` in that order;
`Resolution`, what a zone makes of a wall reading; and the error types `ZoneError`,
`ZoneTimeError` and `TimeParseError`. Every one is an immutable value, `Sendable`, and usable as a
map key. The fields are the representation and may be read; **the makers are the checked way to
make a value**, and a constructor takes its fields as given, as every `data` type's does.

**The calendar is the proleptic Gregorian calendar** with astronomical year numbering: year `0`
exists and is a leap year, and the year before it is `-1`. A day is always 86,400 seconds of
`Instant`: leap seconds are not counted, as the host's clock does not count them.

**Ranges.** A `Duration` and an `Instant` are any `long` of microseconds, about ±292,000 years
(`Instant.Min` is `-290308-12-21T19:59:05.224192Z`, `Instant.Max` `+294247-01-10T04:00:54.775807Z`).
A `Date` is any `int` of days, about ±5,880,000 years (`Date.Min`, `Date.Max`), so every instant has
a date in every zone. A maker, a conversion or arithmetic whose answer would leave its type's range
traps, as an integer overflow does: a `Date` or `DateTime` beyond the instants' range has no
`Instant` and converting it traps; so does a `Period` whose months or days leave `int`.
`Duration.Min`, `Duration.Max`, `DateTime.Min` and `DateTime.Max` are the other bounds.

## Clocks

| Function | Meaning |
| --- | --- |
| `now() -> Instant` | The wall clock: the current point on the timeline, to the microsecond the host gives. Anyone may set this clock, so two readings may differ by any amount, negative included. Stamp things with it. |
| `monotonic() -> Duration` | The monotonic clock: the time since an origin that means nothing, never decreasing within a run. Measure with it: `monotonic() - start` is the time elapsed since `start = monotonic()`. |

**`monotonic()` answers a `Duration`, not an `Instant`**: a reading of it names no point in time, so
it has no calendar, no zone and no text as a date. There is no conversion between the two clocks.

## Durations

`micros`, `millis`, `seconds`, `minutes`, `hours`, `days` and `weeks` each take a `long` count and
answer the exact duration; a day is 24 hours and a week 7 days of timeline, whatever the clocks of a
zone do. Each is also a method on `int` and `long`, so `2.hours == hours(2)` and
`90.seconds == minutes(1) + seconds(30)`. `Duration(micros)` makes one from its count.
`Duration.Zero`, `Duration.Min` and `Duration.Max` are the zero, least and greatest durations.

| Method | Meaning |
| --- | --- |
| `micros` | the count of microseconds (the field) |
| `toDays`, `toHours`, `toMinutes`, `toSeconds`, `toMillis` | how many whole units it holds, a fraction dropped toward zero: `(hours(1) + minutes(30)).toMinutes` is `90`, `seconds(-90).toMinutes` is `-1` |
| `toHoursPart`, `toMinutesPart`, `toSecondsPart`, `toMicrosPart` | the part left in that unit below the next larger: `(hours(50) + minutes(7)).toHoursPart` is `2`; negative for a negative duration |
| `toSecondsReal -> real` | the seconds with their fraction |
| `isNegative`, `isZero` | its sign |
| `abs` | without its sign; `Duration.Min.abs` traps |

**Arithmetic.** Durations add, subtract and negate (`Add`, `Sub`, `Neg`); `d * n` and `d / n`
scale by a `long`, a remainder dropped toward zero, dividing by zero trapping; `d / e` between two
durations answers how many whole `e` fit in `d` as a `long`. A count on the left, `3 * d`, does not
compile: the built-in types' operators cannot gain methods, so the duration is written first.

## Instants

`epochSeconds(n)`, `epochMillis(n)` and `epochMicros(n)` make the instant `n` units after
1970-01-01T00:00:00Z (before it where `n` is negative), `epochSeconds` and `epochMillis` trapping
where the count is too large; `t.epochSeconds` and `t.epochMillis` count back, rounding toward
negative infinity, so `epochMillis(-1).epochSeconds` is `-1`, and the field `t.epochMicros` is the
exact count. The unit is in every name because no default unit is safe. `Instant.Epoch` is the instant
`epochMicros(0)`.

`t + d` and `t - d` move an instant along the timeline by a duration, and `later - earlier` is the
`Duration` between two instants. **An instant has no calendar, so `t + months(1)` does not compile**,
nor does any calendar field: `t.atZone(z)` first.

## Calendar values

**Makers.** `date(y, m, d)`; `time(h, m)`, `time(h, m, s)` and `time(h, m, s, us)`;
`dateTime(y, mo, d, h, mi)`, `dateTime(y, mo, d, h, mi, s)`, `dateTime(y, mo, d, h, mi, s, us)` and
`dateTime(date, time)`. **Numbers the program wrote that name no date or time trap**, naming what
was wrong: `date(2026, 2, 30)` traps with `there is no such date: 2026-02-30`, and `time(24, 0)`,
`time(9, 60)`, `time(9, 0, 60)` (there is no leap second) and a `us` outside `0` to `999_999` trap
naming the field and its range. Text from outside the program is a `Result` ([parsing](#parsing)).
`Time.Midnight` is `time(0, 0)` and `Time.Noon` `time(12, 0)`.

**Fields.** One word per part; a value answers only the parts it has.

| Method | On | Meaning |
| --- | --- | --- |
| `year`, `month`, `day` | `Date`, `DateTime`, `Zoned` | the year, the month `1` to `12`, the day of the month `1` to `31` |
| `weekday -> Weekday` | the same | the day of the week |
| `dayOfYear` | the same | `1` to `366` |
| `daysInMonth`, `daysInYear`, `isLeapYear` | the same | of the date's month and year |
| `monthName` | the same | `January` … `December` |
| `isoWeek`, `isoWeekYear` | the same | the ISO 8601 week (`1` to `53`, weeks starting on Monday, week 1 holding the year's first Thursday) and the year it belongs to: 2027-01-01 is in week 53 of 2026 |
| `hour`, `minute`, `second`, `microsecond` | `Time`, `DateTime`, `Zoned` | `0`–`23`, `0`–`59`, `0`–`59`, `0`–`999_999` |
| `date`, `time` | `DateTime` | the fields |
| `date`, `time` | `Zoned` | its reading's parts |
| `epochDay`, `microsOfDay` | `Date`, `Time` | the fields: days since 1970-01-01, microseconds since midnight |

`isLeapYear(year)` and `daysInMonth(year, month)` are module functions too; `daysInMonth` traps on a
month outside `1` to `12`.

**`Weekday`** is `Monday` … `Sunday`, ordered in that order, ISO 8601's. `w.isoNumber` is `1`
(Monday) to `7` (Sunday), `w.plus(n)` the day `n` days on (any `int`, negative included), and it
displays as its name.

**Replacing a field.** `withYear(y)`, `withMonth(m)` and `withDay(d)` on `Date`, `DateTime` and
`Zoned`; `withHour`, `withMinute`, `withSecond` and `withMicrosecond` on `Time`, `DateTime` and
`Zoned`. `withYear` and `withMonth` clamp the day to the new month's length
(`date(2024, 2, 29).withYear(2025)` is 2025-02-28); a value out of the field's range, and a `withDay`
past the month's end, trap. On a `Zoned` the wall reading changes and the zone resolves it as
[a period does](#zoned-arithmetic).

**Starting points.** `startOfYear`, `startOfMonth` and `startOfWeek` (the Monday) on `Date`,
`DateTime` and `Zoned`, at midnight; `endOfMonth` on the same, the month's last day at the same
time of day; `startOfDay`, `startOfHour`, `startOfMinute` and `startOfSecond` on `DateTime`
and `Zoned` truncate the time of day. On a `Date` the date-only ones answer a `Date`; a `Date` has no
`startOfHour`. A start or end that falls outside `Date.Min` to `Date.Max` traps naming the date
(`Date.Max`, 11 July, has no `endOfMonth`). `d.onOrAfter(w)` is the first date on or after `d` falling on weekday `w`, and
`d.onOrBefore(w)` the last on or before it: `date(2026, 8, 28).onOrAfter(Monday)` is 2026-08-31, and
a Friday's `onOrAfter(Friday)` is itself.

## Periods

A `Period` is a length of calendar: so many months and so many days. **Months and days never
convert**: a month is 28 to 31 days and a day on a zone's clock 23 to 25 hours, so neither is
some number of the other. `months(n)` and `years(n)` take an `int`, and are methods on `int` too
(`3.months`). `years(n)` is `12 * n` months, which is exact, so `years(1) ==
months(12)`; `months(1) != Period(0, 30)`. `Period.Zero` is the empty period.

Periods add, subtract and negate part by part, never carrying days into months, and `p * n` scales
both parts by an `int`; a part leaving `int` traps as an integer overflow does. `p.months` and
`p.days` are the fields; `p.toYearsPart` and `p.toMonthsPart` split the months, each part
carrying the months' sign (`months(14)` is 1 year and 2 months, `months(-14)` -1 and -2), and
`p.isZero` asks for the empty period. A period has no order: whether a month is longer than 30 days
depends on the month. `p.toIsoString` writes ISO 8601's form, `P1Y2M3D`: the years, the months
left over and the days, each where it is not zero, `P0D` for the empty period, and a negative part
with its own sign (`P-1Y-2M`, `P1M-3D`), since the parts may differ in sign; `parsePeriod` reads it
back ([Parsing](#parsing)).

**A duration is exact timeline and may be added to an `Instant`, a `DateTime` or a `Zoned`; a
period is a calendar step and may be added only to a `Date`, a `DateTime` or a `Zoned`.** A `Date`
takes no duration — `date + days(1)` does not compile; `date.plusDays(1)` or `date + Period(0, 1)`
is the day after — and an `Instant` takes no period.

## Arithmetic

| Expression | Answer |
| --- | --- |
| `Instant ± Duration` | `Instant`, moved along the timeline |
| `Instant - Instant` | `Duration` |
| `Date ± Period` | `Date`: the months first, the day clamped to the month it lands in, then the days |
| `Date - Date` | `Period` of days only: `date(2026, 3, 1) - date(2026, 1, 31)` is `Period(0, 29)` |
| `DateTime ± Duration` | `DateTime`, moved along its own wall clock |
| `DateTime ± Period` | `DateTime`, the date moved as a `Date` is, the time of day kept |
| `DateTime - DateTime` | `Duration` along the wall clock |
| `Zoned ± Duration` | `Zoned`, moved along the timeline and read in the same zone |
| `Zoned ± Period` | `Zoned`, moved along the calendar ([below](#zoned-arithmetic)) |
| `Zoned - Zoned` | `Duration` between their instants |
| `Time` | no arithmetic: 23:30 plus an hour is tomorrow or a fault, and a time of day does not say which; `d.atTime(t)` first |

**Month ends.** Adding months clamps the day to the length of the month it lands in, which keeps "the
last day of the month" on a last day: `date(2026, 1, 31) + months(1)` is 2026-02-28 and
`date(2024, 1, 31) + months(1)` 2024-02-29; `date(2024, 2, 29) + years(1)` is 2025-02-28 and
`+ years(4)` 2028-02-29. **Months first, then days**: `date(2026, 1, 31) + Period(1, 1)` is
2026-03-01, by way of 2026-02-28. Clamping makes month steps lossy, so
`date(2026, 1, 31) + months(1) - months(1)` is 2026-01-28.

`plusYears(n)`, `plusMonths(n)`, `plusWeeks(n)` and `plusDays(n)` on `Date`, `DateTime` and
`Zoned` are the calendar steps by name, `+ years(n)`, `+ months(n)`, `+ Period(0, 7 * n)` and
`+ Period(0, n)`; a negative `n` steps back.

**Between dates.** `a.daysUntil(b)` is the whole days from `a` to `b` as an `int`, negative where `b`
is earlier, trapping as an integer overflow does where the count leaves `int` (only between dates
more than about 5,880,000 years apart); `a.monthsUntil(b)` the whole months, counted as a person counts them (2026-01-31 to
2026-02-28 is `0` months, to 2026-03-01 is `1`); `a.until(b)` the `Period` of those whole months and
the days left over, so `a + a.until(b) == b` for `a <= b`. Where `b` is earlier both parts are
negative or zero and the same holds: `date(2026, 3, 1).until(date(2026, 1, 31))` is `-1mo -1d`.
`b.monthsUntil(a)` is always `-a.monthsUntil(b)`, but clamping makes two other round trips fail for
some pairs: `date(2026, 1, 30).until(date(2026, 3, 1))` is `1mo 1d` (by way of 2026-02-28), so
`b - a.until(b)` is 2026-01-31, not `a`, and `b.until(a)` is `-1mo -2d`, not `-1mo -1d`.

### Zoned arithmetic

**A duration moves a zoned reading along the timeline and a period moves its calendar**, and across
a change of the zone's clocks the two differ. In Toronto the clocks went forward at 02:00 on
2026-03-08:

```text
val t = dateTime(2026, 3, 7, 9, 0).atZone(toronto)?
t.plusDays(1)              // 2026-03-08T09:00:00-04:00[America/Toronto]: the same wall clock, 23 hours on
t + days(1)                // 2026-03-08T10:00:00-04:00[America/Toronto]: 24 hours on
(t + days(1)) - t.plusDays(1)   // 1h
```

A period's answer is the wall reading moved by the calendar and resolved in the zone again. A reading
landing in the hour the clocks **skip** is pushed forward past the gap by the gap's length (02:30 on
2026-03-08 becomes 03:30). One landing in the hour the clocks **repeat** keeps the offset it had
where that is one of the two, and otherwise takes the earlier.

## Zones

| Function | Meaning |
| --- | --- |
| `zone(name: string) -> Result[Zone, ZoneError]` | A zone by its name in the tz database (`"America/Toronto"`, `"Asia/Kolkata"`), `"UTC"`, or a fixed offset `"+05:30"`, `"-08:00"`, `"+0530"`, `"Z"`. |
| `utc: Zone` | UTC, a value rather than a call: the one zone that cannot fail to be found. `zone("UTC")? == utc`. |
| `localZone() -> Zone` | The host's own zone: the one the `TZ` environment variable names (a leading `:` dropped; a path is read as the zone's file and named for what follows its `zoneinfo/`), and where `TZ` is unset, the one `/etc/localtime` is, named for what follows the `zoneinfo/` of the link's target (`localtime` where it is a copy rather than a link). It always answers: where the host names no zone it can read — `TZ` empty, naming no zone in the database, not a zone's name, or a file that does not decode — `utc`. |
| `fixedZone(offset: Duration) -> Zone` | A zone whose clocks are always `offset` from UTC; the offset must be whole minutes within ±23:59, and anything else traps. `fixedZone(Duration.Zero) == utc`. |

**A zone's name usually comes from outside the program**, so `zone` answers a `Result`. Its
`ZoneError` is `UnknownZone(name)` where the database has no such zone, `NotAZoneName(name)` where
the text cannot be one — it is empty, starts with `/`, holds a `..` segment, or a character outside
ASCII letters, digits and `/ _ + -` — which is decided before anything is read, and
`Malformed(name)` where the database's file for it cannot be decoded. It is an [`Error`](/spec/errors/#the-error-trait): `e.message` and its display
name the zone: `no zone is named 'Mars/Olympus_Mons'`.

The database is the host's (the directory `TZDIR` names, otherwise `/usr/share/zoneinfo`), kept
current by the operating system; Avin ships none. Offsets are whole minutes. A reading past the last
transition the host's table lists takes the table's last offset.

| Method of `Zone` | Meaning |
| --- | --- |
| `name` | `"America/Toronto"`, `"UTC"`, `"+05:30"` |
| `offsetAt(t: Instant) -> Duration` | how far its clocks are from UTC at `t`: `-4` hours in Toronto in July |
| `isDaylightSavingAt(t)`, `abbreviationAt(t)` | whether `t` falls in daylight saving there, and the clocks' abbreviation then (`"EDT"`); a fixed zone is never in daylight saving and its abbreviation is its name |
| `resolve(dt: DateTime) -> Resolution` | which of the three cases the wall reading is in this zone |
| `isFixed` | whether its offset never changes |

`Resolution` is `Unique(at: Zoned)`, `Repeated(earlier: Zoned, later: Zoned)` where the clocks show
the reading twice, and `Skipped(before: Duration, after: Duration)` where they never show it, with
the offsets either side of the change. Two zones are equal when their names are; a zone displays as
its name.

### Reading in a zone

| Conversion | Answer |
| --- | --- |
| `t.atZone(z)` on an `Instant` | the `Zoned` reading of `t` in `z`; always one |
| `zt.atZone(z)` on a `Zoned` | the same instant read in `z` |
| `dt.atZone(z) -> Result[Zoned, ZoneTimeError]` on a `DateTime` | the reading where `z` shows it once; where it shows it twice or never, `Err` |
| `dt.atZoneEarlier(z)`, `dt.atZoneLater(z)` | always one: in a repeated hour the earlier or the later instant; in a skipped hour both push the reading forward past the gap by its length |
| `d.atStartOfDay(z)` on a `Date` | the first instant of the day in `z`, a skipped midnight pushed past its gap |

**A wall reading a zone skips or repeats is handed back, not guessed at.** `ZoneTimeError` is
`Skipped(reading, zone, before, after)` and `Repeated(reading, zone, earlier, later)`, and its message
says what to write instead: `2026-03-08T02:30:00 never happens in America/Toronto: the clocks go from
-05:00 to -04:00; atZoneLater or atZoneEarlier takes the reading past the gap`, and `2026-11-01T01:30:00
happens twice in America/Toronto, when the clocks go back; atZoneEarlier or atZoneLater picks one`.

A `Zoned` holds its reading, the offset in force (a `Duration`, the one thing that tells the two
readings of a repeated hour apart) and its zone, as the fields `dateTime`, `offset` and `zone`. Its
other methods: `toInstant`, `isDaylightSaving` and `abbreviation`, and every field and step
above; `epochSeconds`, `epochMillis` and `epochMicros` are its instant's counts.

## Conversions

| From | To | Spelling |
| --- | --- | --- |
| `long` count | `Instant` / `Duration` | `epochSeconds(n)`, `epochMillis(n)`, `epochMicros(n)` / `micros(n)` … `weeks(n)` |
| `Instant` | count | `t.epochSeconds`, `t.epochMillis`, `t.epochMicros` |
| `Instant` | `Zoned`, then any calendar value | `t.atZone(z)`, then `.dateTime`, `.date`, `.time` |
| `Zoned` | `Instant`, `DateTime`, `Date`, `Time` | `toInstant`, `dateTime`, `date`, `time` |
| `DateTime` | `Zoned` | `atZone(z)` (a `Result`), `atZoneEarlier(z)`, `atZoneLater(z)` |
| `DateTime` | `Date`, `Time` | `date`, `time` |
| `Date` + `Time` | `DateTime` | `d.atTime(t)`, `t.atDate(d)`, `dateTime(d, t)` |
| `Date` | `DateTime`, `Zoned` | `d.atStartOfDay()`, `d.atStartOfDay(z)` |
| `Date` | count | `d.epochDay`; `Date(n)` back |
| `Time` | count | `t.microsOfDay`; `timeOfMicros(us)` back, trapping outside `0` to `86_399_999_999` |

## Order, equality and hashing

`Instant`, `Duration`, `Date`, `Time`, `DateTime` and `Weekday` are `Ord` by value, so `<`, `max`,
sorting and `TreeMap` keys work, and `Eq` and `Hash` agree. A `Period` and a `Zone` are `Eq` and
`Hash` but not `Ord`. Values of different types never compare: `date(2026, 8, 28) ==
dateTime(2026, 8, 28, 0, 0)` does not compile.

**A `Zoned` is equal to another only with the same instant in the same zone**: 09:00 in Toronto and
22:00 in Tokyo on the same day are one instant and two readings, so they are not `==`.
`a.isBefore(b)`, `a.isAfter(b)` and `a.isSameInstant(b)` compare instants alone. Its `Ord` orders by
instant, then by wall reading, then by the zone's name, so it agrees with `==` and two readings of one
instant are both kept as `TreeMap` keys.

## Text

**Every type implements `Display`**, so a value goes straight into `s"…"` and `println`, and honours
a format specifier's width and alignment. The calendar types write ISO 8601, which sorts as text in
the order it sorts as time:

| Type | Display |
| --- | --- |
| `Instant` | `2026-03-03T14:30:00Z`, `2026-03-03T14:30:00.5Z` |
| `Date` | `2026-03-08`; `+10000-01-01`, `0000-01-01`, `-0001-12-31` outside years `0` to `9999` |
| `Time` | `09:30:00`, `09:30:07.25` |
| `DateTime` | `2026-03-08T09:30:00` |
| `Zoned` | `2026-03-08T09:00:00-04:00[America/Toronto]`; a fixed zone does not name itself again: `…T14:30:00Z`, `…T20:00:00+05:30` |
| `Duration` | `5400s`, `-45s`, `0s`, `90.5s`, `250ms`, `1.5ms`, `7us` |
| `Period` | `1y 2mo 3d`, `14d`, `0d` |
| `Zone` | `America/Toronto` |
| `Weekday` | `Wednesday` |

The seconds are always written and the fraction only when there is one, without trailing zeros, so
a column of readings has one shape until something below the second appears. A duration is written
in the largest of `s`, `ms` and `us` that gives it a whole part, with the rest as a decimal fraction
without trailing zeros, as it is today; a period keeps months and days apart. `Debug` writes a
duration as `Duration(90.5s)`, a period as `Period(1y 2mo)` and a zone as `Zone(America/Toronto)`, and every
other type as its display.

### Parsing

| Function | Reads |
| --- | --- |
| `parseDate(text) -> Result[Date, TimeParseError]` | `YYYY-MM-DD`, with a sign and more digits for a year outside `0` to `9999` |
| `parseTime(text) -> Result[Time, TimeParseError]` | `hh:mm`, `hh:mm:ss`, `hh:mm:ss.f` with one to six digits of fraction |
| `parseDateTime(text) -> Result[DateTime, TimeParseError]` | a date and a time joined by `T` or one space |
| `parseTimestamp(text) -> Result[Instant, TimeParseError]` | a date-time and an offset: `Z`, `±hh:mm` or `±hhmm` |
| `parseZoned(text) -> Result[Zoned, TimeParseError]` | a timestamp and, optionally, a zone name in brackets |
| `parsePeriod(text) -> Result[Period, TimeParseError]` | `P`, then years `Y`, months `M` and days `D` in that order, at least one, each with its own `-` where negative: what `toIsoString` writes |

**Every display of `Instant`, `Date`, `Time`, `DateTime` and `Zoned` reads back** to an equal value,
and so does every period's `toIsoString`. A year of four digits has no sign; one written with a sign
has four digits or more (`+10000`, `-0001`, `+2026`). A fraction means what it says: `.5` is half a
second. **A parse checks the calendar, not only the shape**: `2026-02-30` has the right shape and is
refused, as are `24:00`, a second of `60`, a seventh digit of fraction (even a `0`), an offset beyond
±23:59, and a timestamp with no offset (defaulting one would invent it). The whole text is read for
its shape first, and then its fields are checked in the order they are written, so
`2026-02-30T24:00` is refused for its day. Without a zone name `parseZoned`'s zone is the offset's
fixed zone (`utc` for `Z` or any zero offset); with one, the offset must be the one that zone's clocks
show at that instant, so both readings of an autumn overlap are read and a reading in a spring gap is
refused for its offset. A period's years are twelve months each, and `P12M` reads as `P1Y` does.
Digits are ASCII only; any other character, a digit of another script included, is a shape error.

`TimeParseError` is `BadShape(text, at, expected)`, `OutOfRange(text, field)`, `Trailing(text, at)`
and `UnknownZone(text, name)`, `at` a byte offset into `text`. `field` is `year`, `month`, `day`,
`date` (a date past `Date.Min` or `Date.Max` in their own years), `hour`, `minute`, `second`,
`fraction`, `offset`, `instant` (a timestamp past `Instant.Min` or `Instant.Max`), `months` or
`days`. Its message quotes the text and says what was expected and where: `cannot read
"2026-02-30": day is out of range`, `cannot read "2026/03/08": expected '-' at byte 4`, `cannot read
"2026-03-08T": expected the end at byte 10`, `cannot read "…[Nowhere/Atall]": no zone is named
'Nowhere/Atall'`. A parse never traps.

### Patterns

`format(pattern)` on `Date`, `Time`, `DateTime` and `Zoned` writes a reading the way a person wants
to read it. A run of one letter is an element, text between `|` bars is written as it stands (`||` is
a bar, between the bars too: `|a||b|` writes `a|b`), and every other character, letters that are no
element included, stands for itself. A run of `h` followed by `12` is one element, the hour on the
twelve-hour clock:

| Element | Writes |
| --- | --- |
| `Y` `YY` `YYYY` | the year as written (`-1`, `12345`), the last two digits of its magnitude (`26`, `01` for `-1`), four digits padded and signed outside `0` to `9999` as [the display](#text) writes it |
| `M` `MM` `MMM` `MMMM` | the month: `8`, `08`, `Aug`, `August` |
| `D` `DD` `DDD` | the day of the month, `5` or `05`; the day of the year, three digits padded (`005`, `240`) |
| `WWW` `WWWW` | `Fri`, `Friday` |
| `h` `hh` | the hour `0` to `23`, `9` or `09` |
| `h12` `hh12` | the hour `1` to `12`: midnight and noon are `12` |
| `a` `A` | `am` or `pm`, `AM` or `PM`: `am` before noon |
| `m` `mm` `s` `ss` | the minute and the second |
| `f` … `ffffff` | the fraction of a second to that many digits, cut rather than rounded |
| `z` `zz` `zzz` `zzzz` | `-04:00`, `-0400`, `EDT`, `America/Toronto`; UTC's are `+00:00`, `+0000`, `UTC`, `UTC`, and a fixed zone's abbreviation and name are its offset |

```text
val t = dateTime(2026, 8, 28, 17, 24, 15).atZone(toronto)?
t.format("WWWW, MMMM D, Y |at| h12:mm a")   // Friday, August 28, 2026 at 5:24 pm
t.format("YYYY-MM-DDThh:mm:ss.fffZ")       // 2026-08-28T17:24:15.000Z
```

**An element the value does not have is refused, never zero-filled**: `date.format("hh:mm")` traps
saying a date has no time of day, and `z` on a `DateTime` traps saying only a zoned reading knows its
zone. An instant has no `format`; read it in a zone first. A pattern is the program's own text, so a
malformed one traps naming the fault: a run of a letter no element has that length of (`YYY`: `a
year is Y, YY or YYYY`), or a `|` that never closes. The whole pattern is read before anything is
written, so a malformed pattern traps as malformed whatever value it is given, before an element the
value lacks: `date.format("hh YYY")` traps naming `YYY`. Names are English; there are no locales.

## Sleeping and timers

A delay is a `Duration` throughout the library: `threads.sleep(d)`, an actor's `after(delay, m)` and
`every(interval, m)`, `ask`'s timeout, a command's `withTimeout(d)`, a socket's `setTimeout(d)`.
`Future.await` and `Condition.waitFor` take a `Duration` beside their count of milliseconds. Every
timer and deadline runs on the monotonic clock, so setting the wall clock moves none of them; a
deadline is `monotonic() + d`.
