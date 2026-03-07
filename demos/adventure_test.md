[STATE]
ID=START
TITLE=THE BEGINNING
TEXT
You stand at the edge of a **digital abyss**. The air smells like *ozone* and *stale coffee*.

```
SYSTEM STATUS: NOMINAL
USER_ID: 0x55FF
```

> "Don't look down," a voice whispers.
ENDTEXT
OPT=Look down anyway|LOOK_DOWN
OPT=Step forward into the light|STEP_FORWARD

[STATE]
ID=LOOK_DOWN
TITLE=THE FALL
TEXT
You looked down. The code stretches into infinity. You feel a sense of **vertigo** before the loop catches you.
ENDTEXT
OPT=Restart|START

[STATE]
ID=STEP_FORWARD
TITLE=THE VOID
TEXT
You step forward. The world dissolves into a *calm white silence*. You have reached the end of this test.
ENDTEXT
OPT=Restart|START
