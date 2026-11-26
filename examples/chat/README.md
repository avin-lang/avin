# chat

A chat room of actors. The `Room` actor owns who is present and the last lines said, and publishes
what happens on a `Topic`; each `User` actor subscribes through an adapter that turns the room's
events into its own messages, and keeps what it has heard. A whisper goes straight to one member, a
second user with a name already taken is refused, and a newcomer is sent the lines said before it
came. The program plays a short conversation, each step an `ask` the room answers once the step is
done, so every user's transcript is the same on every run.

```
avin run .
```
