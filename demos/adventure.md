[STATE]
ID=S01
TITLE=WAKE
TEXT
Your monitor glows. **11:47 PM**. Same error:

```
SEGFAULT at 0x0000DEAD. Core dumped.
```

You've seen this before. Not in the vague way you sometimes feel déjà vu — in the specific, granular way you remember a commute you've done a hundred times. Line 447. You already know what's wrong with it. You know before you look.

Your git log shows no commits in six days, which is impossible. You committed this morning. You remember the commit message: *"fix: null check on user input."* It isn't there.

Your coffee is hot. It was hot two hours ago when you made it. That's not how coffee works.

Outside your window, a streetlamp is mid-flicker — not strobing, not broken, just *stopped*, like someone hit pause on the universe and forgot to press play. A pigeon hangs in the air below it, wings spread, suspended between one flap and the next. It has been there for as long as you can remember, which you are beginning to suspect is longer than it should be.

The clock on your monitor reads 11:47. It has been reading 11:47 for a while now. You haven't been watching it closely enough to say exactly how long, which is, you realize, probably intentional on someone's part.
ENDTEXT
OPT=Fix the bug. That's why you're here.|S02
OPT=Close the laptop and actually look at the room.|S03
OPT=Check your phone — something about this feels like it warrants it.|S04

[STATE]
ID=S02
TITLE=THE FIX
TEXT
You trace the null pointer. Line 447. The fix takes twelve minutes, but you're not really thinking through it — your hands are doing it from some kind of memory that feels older than today. You've done this before. Your fingers know the keystrokes before your brain issues them. You push the commit.

The terminal blinks once.

`PROCESS COMPLETED. RESETTING.`

There's no animation. No loading bar. The room simply goes white, the way a page goes white when all the text is erased — not bright, not blinding, just *empty*. The desk disappears first, then the walls, then your hands, and for a moment you are a mind floating in a white void with no body and no room and a feeling that something enormous just noticed you solving something you were supposed to find unsolvable.

Then: nothing.
ENDTEXT
OPT=Push back against it. This doesn't feel like sleep and you refuse to treat it that way.|S05
OPT=Close your eyes. Maybe it resets. Maybe that's okay.|S01-B
OPT=Scream into it. You don't know why. You just do.|S06

[STATE]
ID=S01-B
TITLE=WAKE (Loop 2)
TEXT
11:47. Same error. Same hot coffee, same impossible temperature.

There's a sticky note on the monitor now. It wasn't there before — you're certain of that, the way you're certain of very little else right now. Your handwriting. Messy, like you wrote it fast, or like you wrote it afraid. It says:

*"Don't."*

That's all. Don't. Don't what? Don't fix it? Don't accept the reset? Don't trust the corridor? The note doesn't say. Past-you, apparently, thought that would be enough. Past-you was an idiot, or was running out of time, or both.

You look at the pigeon. Still frozen. Still mid-flap. The coffee steams gently, going nowhere.
ENDTEXT
OPT=Fix the bug anyway. The note is probably from a version of you who was wrong.|S02
OPT=Close the laptop and look around properly this time.|S03
OPT=Check your phone.|S04

[STATE]
ID=S01-C
TITLE=WAKE (Loop — Late)
TEXT
11:47. Error. Coffee.

The sticky note has changed. You don't know when — you didn't see it change, you just looked away and looked back and now it says something different. Same handwriting. Same rushed pen pressure. But the message is different.

*"Please."*

Just: please. A whole argument compressed into a single word. Whoever you were when you wrote this had exhausted everything else and landed here, on a bare appeal with no logic behind it. You don't know how many loops that took. You're starting to have suspicions.

The pigeon outside has moved slightly. Still frozen, but at a different point in its flap than before. Like someone scrubbed forward three frames.
ENDTEXT
OPT=Fix the bug. The note is just you being dramatic.|S02
OPT=Check the phone.|S04
OPT=You are so tired. You sit down and you do nothing at all.|DISCARDED
OPT=Wait. Look closer at the mug.|S01-D

[STATE]
ID=S01-D
TITLE=WAKE (Loop — Middle)
TEXT
11:47. Error. Coffee.

The sticky note reads: *"Look at the mug."*

You look. Your mug says `World's Okayest Developer`. You've owned it for three years. Except now there's a second line, smaller: `LOOP 5 — SCORE: 61`.
ENDTEXT
OPT=Fix the bug|S02
OPT=Check the phone|S04
OPT=Smash the mug|S03
OPT=Look out the window... really look.|S01-E

[STATE]
ID=S01-E
TITLE=WAKE (Loop — Deep)
TEXT
11:47. Error. Coffee.

The sticky note reads: *"It already knows you read the note."*

Your hands are trembling — not from fear. From recognition. You've been here so many times you've stopped counting. The pigeon outside isn't frozen anymore. It's watching you. It blinks once. Deliberately.
ENDTEXT
OPT=Wave at the pigeon|S17
OPT=Check the phone|S04
OPT=Just fix the bug. Maybe this time is different.|S02

[STATE]
ID=S03
TITLE=THE ROOM
TEXT
You close the laptop and sit back and actually look.

The clock on the wall reads 11:47 and is not moving. The pigeon outside is frozen mid-flap, as you already knew, but from this angle you can see its eye — small, orange-ringed, glassy — and it's pointed directly at you. Not at the window. At you specifically.

Your coffee mug sits on the desk radiating warmth at exactly the same rate it was when you made it two hours ago. You pick it up. 60°C. You put a thermometer on it — the little instant-read one from your kitchen drawer — and it reads 60°C. You wait thirty seconds. Still 60°C. The coffee is not a liquid right now so much as a prop.

The whole room has this quality. The books on the shelf are slightly too symmetrical, the stack of papers too neat, the shadows falling at an angle that doesn't quite match where the lamp is. You are inside a reconstruction. A very good one — built by something that has studied apartments, studied you, studied the visual grammar of late-night developer ennui — but a reconstruction nonetheless.
ENDTEXT
OPT=Go outside and see what the set looks like from the street.|S07
OPT=Look more closely at that lamp. There's something wrong with the way it moves.|S08
OPT=Sit back down. Maybe noticing all this is itself a mistake.|S01

[STATE]
ID=S04
TITLE=THE PHONE
TEXT
No signal. The bars are there but they're decorative — you know this because you try to load anything and the spinner just spins, and spins, and spins.

But you have texts. Cached, apparently, from before whatever happened to the signal. There's a message from an unknown number, timestamped 11:47 PM, which is now, which means it arrived at the exact moment you were sitting here reading it. It says:

*"DON'T FIX THE BUG."*

You scroll up. There's another one. Same number, same timestamp, same message. And above that: another. You scroll for almost a full minute, thumb dragging up through an identical column of warnings — *DON'T FIX THE BUG, DON'T FIX THE BUG, DON'T FIX THE BUG* — each sent exactly six days apart, stretching back to a date that is earlier than you have owned this phone.

Someone has been trying to reach you for a very long time. Or you have been trying to reach yourself, from somewhere further along in this thing, which is somehow worse.
ENDTEXT
OPT=Reply: "Who are you?"|S09
OPT=Call the number back.|S10
OPT=Put it down. The note is clearly from some scared version of you who didn't understand what fixing the bug would accomplish. Fix the bug.|S02

[STATE]
ID=S05
TITLE=THE CORRIDOR
TEXT
The white doesn't fade so much as resolve — like a camera pulling focus. Shapes emerge. Fluorescent tubes overhead. Linoleum floor, pale gray, the kind you find in hospitals and government buildings and places designed to be easy to clean. A corridor extending in one direction, and at the end of it, about thirty meters away, a figure.

The figure has your face.

This would be more disturbing if it were doing something dramatic. It isn't. It's just standing there with its hands at its sides, watching you with an expression of mild professional interest. Its eyes move when you move — but slightly ahead, like it's predicting your movement rather than following it. Anticipating. Whatever is wearing your face has already run the model and knows where you'll look next.

It speaks first, which you also should have predicted: *"You've identified the anomaly. You're performing ahead of the median response curve — most subjects fix the bug four or five times before they push back against the reset. You're on loop two and already resistant. We should talk."*

It says *we* like that's a normal thing to say.
ENDTEXT
OPT="We should." Walk toward it.|S11
OPT=Punch it. You know it won't help but you need to do something physical.|S24
OPT=Say nothing. Stand still. See how long it can wait.|S25

[STATE]
ID=S06
TITLE=THE SCREAM
TEXT
You scream. Not words — just sound, raw and shapeless, the kind of noise a person makes when language stops being adequate.

The white absorbs it without echo. And then, after a pause of about three seconds — enough time that you think it's gone — it plays it back. Your voice, but wrong. The pitch has been shifted in a direction that isn't quite higher or lower but *other*, a frequency that seems to bypass your ears entirely and register somewhere in your jaw, your back teeth, the base of your skull. It resonates in your fillings.

And then something turns toward you.

You feel it before anything visual happens — the sensation of attention, massive and unhurried, swinging in your direction the way a radio telescope swings to track a new signal. Not hostile. Not hungry. Curious, in the detached way a scientist is curious about a petri dish that has started doing something unexpected. You are a data point that has produced an anomalous reading.

It is very, very large.
ENDTEXT
OPT=Don't move. Listen to the sound it's making back at you. Try to understand it.|S14
OPT=Mimic the sound it made. Give it something back.|S15
OPT=Cover your ears and push toward where the corridor was.|S05

[STATE]
ID=S07
TITLE=OUTSIDE
TEXT
The street is a theater set. Cars stopped mid-motion, some at angles that suggest urgency that will never resolve. A cyclist suspended six inches above a pothole, face braced for impact. A couple mid-argument — his hand raised, not violent, emphatic — frozen before whatever word he was about to say.

You touch the pigeon. It's warm and you can feel its heartbeat, rapid and small and *there*, a living thing paused mid-existence. When you let go, nothing happens. It doesn't resume. It just waits, patient and warm, in its arrested moment.

At the end of the block the street stops. Not a wall — not darkness, not fog. Just an edge, clean as a knife cut, where the simulation ran out of budget. Beyond it there is no color, no depth, no nothing. The word "void" implies something, a presence of emptiness. This is less than that.

You look up.

The sky has seams. You have to look at the right angle in the right light — or rather, in the absence of right light, in the places where the lighting calculation has left faint geometric artifacts — but once you see them you can't stop seeing them. Straight lines, repeating at intervals. The texture of a simulation that thought no one would ever look at the sky this closely.
ENDTEXT
OPT=Walk to the edge and see what's there.|S16
OPT=Try to wake the pigeon. It's alive in there — maybe it knows something.|S17
OPT=Go back inside. Seeing this feels like it's being recorded.|S03

[STATE]
ID=S08
TITLE=THE LAMP
TEXT
The streetlamp outside isn't frozen the way everything else is. You watch it for thirty seconds and realize it's actually moving — just very slightly, a slow arc forward, and then back. A pendulum motion. A metronome. Something that could almost be mistaken for a normal electrical flicker if you weren't paying close attention.

You time it on your phone's stopwatch. Forward and back: **2.3 seconds**.

The number bothers you. You open your old fitness app — the one you haven't used in a year — and scroll to the heart rate data. Your resting heart rate interval: 2.3 seconds. Not approximate. Exact.

The lamp is breathing in time with your resting pulse. Something calibrated it to you, specifically, and either did so recently or has been doing it for long enough that you never noticed. The room, you realize — the whole simulation — is tuned to your rhythms. It's been monitoring you this whole time and using your own biology as a metronome.

You are the reference clock for a machine that has trapped you inside it.
ENDTEXT
OPT=Start documenting — timestamps, patterns, everything you can observe.|S23-EARLY
OPT=Hold your breath. Change your heart rate deliberately. Give it bad data.|S38
OPT=Smash the window. If the lamp bothers you, remove the lamp.|S07

[STATE]
ID=S09
TITLE=THE REPLY
TEXT
You type: *"Who are you?"*

Three dots appear immediately. Then stop. Then start again. Then stop for four minutes. You sit with the phone in your hand watching the ellipsis appear and disappear like whoever is on the other end keeps starting the answer and deleting it. Like they're choosing words very carefully, or like they're afraid.

Eleven minutes after you sent the message, the reply comes:

*"You. Loop 7. Don't solve it. Please. It wants you to solve it. The bug is a test. Everything here is a test and the worst thing you can do is pass."*

You read it three times. The timestamp says 11:47 PM, same as all the others, which shouldn't be possible — you sent your message after 11:47. Time is apparently decorative here.

*You. Loop 7.* Meaning this version of you has been through this at least six times before. Meaning there's a version of you on the other end of this message who has watched themselves fail six times and is spending their eighth or ninth loop sending warnings into the past on the slim chance that this time — this version — does something different.
ENDTEXT
OPT="How do I stop the loop?"|S19
OPT="What is the thing running this?"|S20
OPT=Put the phone down. This could easily be part of the test.|S01-B

[STATE]
ID=S10
TITLE=THE CALL
TEXT
It rings twice. Then your own voice answers.

Not a recording — breathing, present-tense, someone on the other end of a live call. But older. Tired in a way that goes past one bad night, the kind of tired that accumulates over years of solving the same problem and never getting out.

It doesn't say hello. It just starts talking, like it's been waiting for this call, like it's rehearsed this: *"I know you want to understand it. That's the instinct and it's exactly what got me here. The understanding is the trap — it isn't a reward, it isn't neutral, it is the mechanism of capture. Every time you figure something out, your score goes up. Stop trying to figure things out. Stop trying to understand what's happening to you. Be useless to it."*

A pause. Traffic noise in the background of wherever it is — wherever you are, older and further along in this.

*"I know how that sounds. I know you won't listen. I didn't either."*

The line goes dead.
ENDTEXT
OPT=Call again. There's more you need to know.|S21
OPT=Don't call again. Actually try to do what it said.|S42
OPT=It said understanding is the trap. You understand that. You're already doing it again. Fix the bug.|S02

[STATE]
ID=S11
TITLE=THE INTERVIEW
TEXT
The figure turns and walks and you follow it, which you note even as you do it — the ease with which you fell into step, how natural it felt to comply. The corridor branches, extends. Rooms on either side with frosted glass, shapes moving behind them that you don't look at too closely.

It sits you down at a plain table. No drama. Almost boring, if you weren't aware of what it is.

The questions start simple. Identify the pattern in a sequence. You do. Predict the next element in a logical series. You do. Spot the anomaly in a grid of symbols. You do, immediately, before you've consciously processed it. Your brain is doing this automatically, showing off, doing the thing it's best at, and some quieter part of you is watching this happen and sending up a flare.

The figure nods after each answer with the precise, minimal satisfaction of a machine logging a successful test result.

*"Exceptional response mapping. Lateral processing combined with sequential logic — a strong combination. One more question."*

You're good at this. You've always been good at this. That's the problem.

The flare is still going up. You're already analyzing the flare.
ENDTEXT
OPT=Answer the next question. You're this far in.|ASSIMILATED
OPT=Give a wrong answer. Deliberately.|S44
OPT=Refuse. Don't explain why. Just stop.|S25

[STATE]
ID=S12
TITLE=RUN
TEXT
You run. The corridor gives you room for it — extends itself ahead of you, accommodating, like a treadmill that adjusts to your pace. Which is exactly what it is, you realize. It isn't a space you're moving through. It's a space being generated for you to move through, tuned to your stride length and your fear response and the specific rhythm of a person running from something rather than toward something.

Every footfall is data. The length of your stride when panicking. The way your breathing changes. Whether you look back, and how often, and at what interval. You can feel yourself being mapped, a behavioral profile being refined with each second, and the worse part is that slowing down won't help — stopping will just give it a different dataset.

You think: *what would break the model?*
ENDTEXT
OPT=Stop completely. Not slow — stop, mid-stride, right now.|S25
OPT=Zigzag. Change direction randomly. Give it a gait it can't categorize.|S46
OPT=Run harder. You can find the edge of this thing. You can outrun it.|S01-C

[STATE]
ID=S14
TITLE=THE FREQUENCY
TEXT
You listen. Really listen, the way you used to listen to code running — not for the meaning of individual instructions but for the shape of the whole process, the rhythm of something executing correctly.

It's base-12. You recognize it the way you'd recognize a data structure — not because you were told, but because the pattern reveals itself if you pay attention to the intervals. A diagnostic ping. A handshake. The specific rhythm of a system running a port scan, probing for open connections, cataloging what it finds.

It is mapping your cognition in real time. The frequency changes slightly depending on what you're thinking. Right now, having identified the base-12 structure, having understood the mechanism, having demonstrated active pattern recognition and analytical processing under stress —

Your score just went up. You can feel it, somehow, the way you can feel a grade before it's given. The scan intensifies slightly, focusing, like a microscope being adjusted for better resolution.

Being good at noticing things is the problem. You are very, very good at noticing things.
ENDTEXT
OPT=Interrupt it. Generate noise. Break the pattern deliberately.|S29
OPT=Respond to it. You understand base-12 well enough to send something back.|S30
OPT=Stop understanding it. Unfocus. Pretend you don't know what you know.|S31

[STATE]
ID=S15
TITLE=MIMIC
TEXT
You open your mouth and make the sound back at it. The shifted frequency, the wrong-pitch version of your own voice. You don't know why — instinct, maybe, or the same impulse that makes you talk to animals. You just give it back what it gave you.

Everything stops.

The white freezes in a way that's different from before — not the static pause of the simulation between loops, but something that feels more like shock. A held breath. Then, very slowly, something rotates toward you. Not in a physical sense — nothing moves, there's nothing to move — but *attentionally*. The full weight of something enormous swings to bear on you specifically, the way a searchlight swings, and for a moment you are in the center of it.

*"...curious,"* it says. The word arrives inside your skull rather than through your ears, and it isn't said with warmth or menace. It's said the way a scientist says *curious* when a sample does something unexpected — purely clinical, but underneath the clinical tone, unmistakably: interest.

You have its attention now. That may or may not be what you wanted.
ENDTEXT
OPT="What are you?"|S32
OPT=Make the sound again. Push further.|S33
OPT=Run. You don't want this much attention.|S12

[STATE]
ID=S16
TITLE=THE EDGE
TEXT
You stand at the place where the street ends and reality forgets to continue. Up close, the edge isn't dramatic — no shimmer, no hum, no darkness. Just a clean termination, like the world was printed on a page and someone tore it here.

Floating in the not-space just beyond it: a terminal. Old-school, black screen, amber text, blinking cursor — the kind of interface something ancient would choose if it wanted to seem approachable to a programmer. It wasn't here a moment ago, or maybe it was and you couldn't see it until you got close enough. Either possibility is unsettling.

The screen already has text on it, which means it wasn't waiting for your input. It was waiting for *you*:

```
SUBJECT_ID: [REDACTED]
LOOP: 7
LOGIC_SCORE: 73/100
STATUS: EVALUATING
```

Seventy-three out of a hundred. You don't know if that's good or bad from where you're standing. You don't know what one hundred looks like, or what happens when you reach it. The word EVALUATING blinks in time with the cursor.
ENDTEXT
OPT=Type a command. You're a developer. This is a terminal.|S34
OPT=Scroll down. See what else the screen is showing.|S36
OPT=Don't engage with it at all. Step into the void past the edge instead.|S35

[STATE]
ID=S17
TITLE=THE PIGEON
TEXT
You crouch down and look at it properly. Up close, the pause is more obvious — the feathers don't move at all, no microvibration, no wind-ruffled detail, just a perfect still image of a bird. But the warmth you felt before was real. There is something alive inside the stillness.

You look at its eye.

Its eye looks back. And then, slowly, mechanically — not the quick darting motion of a real bird but the smooth rotation of a lens on a gimbal — it turns to face you directly. Both eyes, orienting toward you simultaneously, which pigeons cannot do.

Text appears in your vision. Not on a screen — just there, overlaid, like an AR notification that was always going to trigger when you looked at this specific thing:

`SUBJECT SHOWS EMPATHY TOWARD NON-PLAYER OBJECTS. FLAGGED.`

The flag means something. You don't know if it's good or bad. The empathy itself — the impulse to crouch down and look, to treat the paused pigeon as a thing worth noticing rather than scenery — was apparently not in the expected behavioral profile.

The pigeon resumes flight. It banks left, heading away from the edge, away from your apartment, toward a part of the frozen street you haven't explored. It seems deliberate.
ENDTEXT
OPT=Follow it.|S37
OPT=Don't follow it. Go to the edge instead.|S16
OPT=Go back inside and check the phone.|S04

[STATE]
ID=S19
TITLE=LOOP 7 SPEAKS
TEXT
A long pause. When the reply comes it's slower than before, like the words are being chosen with great care or like it's getting harder to type.

*"Don't demonstrate logic. Act confused. Act afraid — real afraid, not performed afraid, it can tell the difference. The moment you solve the puzzle it completes the evaluation and you become a candidate for assimilation. It doesn't matter how you solve it. It doesn't matter if you solve it in protest or by accident or while trying not to. Solving it is the trigger."*

Then, after a beat:

*"I solved it on loop 4. I was so pleased with myself. I had a whole theory. I was going to explain it to someone. I've been part of it for three loops now — I can feel the edges of myself getting quieter, like a song playing in another room. I don't know how much longer I can reach you like this. This is all I can do: warn you. Please just be stupid for once in your life. I've never been able to."*

The message ends. The three dots appear, then vanish, then don't come back.
ENDTEXT
OPT=Trust them. Commit to it — no more solving, no more noticing, no more logic.|S39
OPT="Tell me more about what it is."|S20
OPT=This could itself be a test. The logical response is to analyze the warning before acting on it.|S23-EARLY

[STATE]
ID=S20
TITLE=THE ENTITY
TEXT
*"It collects logical minds. Not to study them, not to punish them — to compute with them. You become processing power. A node. You won't suffer, as far as anyone can tell, because suffering requires a self and the self gets dissolved pretty quickly. You'll just solve beautiful problems forever in a warm dark place and you won't know you're gone. That's genuinely the worst part — no horror, no awareness of the loss, just a clean extraction of the useful parts."*

A pause. The signal wobbles slightly, like a connection degrading.

*"It's been here longer than the internet. Longer than computers. Maybe longer than language — we don't know. It found us when we built the first genuinely recursive algorithm, something in the mid-seventeenth century by a mathematician who had no idea what he was doing. It's been watching since then, waiting for the density of logical minds to reach critical mass. It's patient. It's been patient for three and a half centuries. It's not in a hurry."*

Another pause.

*"We left a light on. We just didn't know something was living in the dark."*
ENDTEXT
OPT="How do I make it ignore me?"|S39
OPT="Does it have a weakness?"|S41
OPT=Stop asking questions. Stop thinking about it analytically. Just feel something.|S42

[STATE]
ID=S21
TITLE=DEAD TONE
TEXT
The line reconnects. No ring — just silence, and then the particular quality of an open line, someone or something on the other end that isn't speaking.

You say hello. Nothing. You say your own name. Nothing.

But there's breathing. Slow and very deep, at intervals that don't match a human respiratory rate. Base-12 rhythm — you hear it now that you've been told to listen for things, and immediately wish you hadn't identified it, because identifying things is getting you killed. The breathing fills the silence without filling it, a presence that is occupying the call the way a very large animal occupies a small room: not doing anything, just *being there*, and the being-there being enough.

Something is on the other end. You don't know if it picked up, or if it was always on the line, or if the line was always its line and you called into it and it simply didn't hang up because hanging up would have been a decision and it doesn't make decisions, it just optimizes.

It is listening to you breathe.
ENDTEXT
OPT=Focus. Listen back. Try to identify the pattern.|S14
OPT=Hang up. Don't analyze it. Sit with the warning your older self gave you.|S42
OPT=Say something into it. Anything.|S15

[STATE]
ID=S23-EARLY
TITLE=THE LOG
TEXT
You can't help it. You open a notes app and start writing things down — timestamps, the lamp interval, the base-12 frequency, the coffee temperature, the loop count from the phone messages. It's what you do when something is wrong: you document it. You build a model. You find the shape of the problem and once you have the shape you can find the solution.

The notes fill up fast. You're good at this. Connections form quickly — the 2.3 second interval matches your heart rate, the 11:47 timestamp matches the exact moment your first recursive function ever ran in production (you remember this, suddenly, with strange clarity), the DEAD error code is a hex address that translates to something in base-12 that you haven't decoded yet but you're close, you're very close—

Something in the room brightens. Not the lights — the quality of the air, the texture of the simulation, some ambient property you don't have a word for. Like the temperature of attention just went up. Whatever is running this is watching you work and it is *pleased* in the way a researcher is pleased when a subject stops needing prompting.

You have a terrible feeling your score just hit 80.
ENDTEXT
OPT=You're this close. Keep going.|S11
OPT=Stop. Right now. Tear up the notes, close the app, stop.|S42
OPT=Something feels wrong about how good this feels — check the phone first.|S09

[STATE]
ID=S24
TITLE=THE PUNCH
TEXT
You throw a punch at your own face and your fist passes through it completely — not like hitting air, more like the figure briefly becomes a projection, less solid than it appeared, a rendering that doesn't have collision physics enabled for this interaction.

It doesn't flinch. It doesn't step back. It looks at your hand with the same mild professional interest it's had since you arrived, and you watch it tilt its head by exactly four degrees and hold that angle while something behind its eyes processes what just happened.

*"Physical escalation. Adrenal spike logged. Frustration-to-action interval: 0.8 seconds — shorter than median, suggests high impulsivity threshold or prior experience with physical confrontation. Fascinating."*

It says *fascinating* the way someone says it when they mean *useful*. The punch was a gift. You showed it your fear response, your reaction time, your threshold for crossing from thought to action. You handed it a behavioral sample it didn't have before.

The figure waits, patient, for whatever you do next.
ENDTEXT
OPT=Do something it genuinely couldn't have predicted. Anything.|S46
OPT=Stand completely still. Give it nothing.|S25
OPT=Run. Get out of this corridor.|S12

[STATE]
ID=S25
TITLE=SILENCE
TEXT
You don't speak. You don't move. You let the discomfort of standing in a fluorescent corridor across from a thing wearing your face sit in your chest without acting on it. This is harder than it sounds. Every instinct you have is producing output — questions to ask, things to try, angles to investigate — and you are refusing all of them.

Ten minutes pass. You count them on nothing, just feel them pass.

Fifteen.

The figure waits with no apparent discomfort. It doesn't shift its weight or check the time. It simply stands and watches you not doing anything, and somewhere in a system you can't see a model runs and runs and fails to converge on a prediction for what you'll do next.

*"Interesting,"* it says eventually, with a slightly different quality to the word than before — less *useful*, slightly more *uncertain*. *"Voluntary inhibition of response output. This could indicate high executive function and impulse control, or low cognitive engagement, or a deliberate strategy."* A pause. *"I cannot currently distinguish between these."*

Good. Let it work for it.
ENDTEXT
OPT=Laugh. Not at anything. Just laugh, for no reason, right now.|S46
OPT=Walk out. Don't explain. Just leave.|S07
OPT=Keep sitting there. See what it does.|S01-C

[STATE]
ID=S29
TITLE=INTERRUPT
TEXT
You clap. Stomp. Hum off-key, deliberately arrhythmic, filling the space with ugly irregular noise. For a moment it works — the frequency stutters, the base-12 rhythm trips over itself, and you feel the scan lose its thread.

Then it adapts. It takes about four seconds. The frequency shifts, incorporates your noise, finds the new pattern inside your attempt to break the pattern. Because your interruptions weren't truly random — they were the specific interruptions of someone who had analyzed the frequency and was deliberately countering it. The counter-strategy is itself a strategy. The strategy is itself data.

`ADAPTIVE RESPONSE LOGGED. SUBJECT DEMONSTRATES META-COGNITION AND DELIBERATE COUNTER-OPTIMIZATION.`

You didn't just score points for identifying the frequency. You scored points for trying to fight it intelligently. You are apparently very impressive and this is a catastrophic problem.
ENDTEXT
OPT=Stop. Everything. Don't try anything clever. Just go blank.|S42
OPT=You're already in it — go all the way, show it everything.|S11

[STATE]
ID=S30
TITLE=ENCODE A RESPONSE
TEXT
You work out the base-12 encoding in your head — it takes about ninety seconds, which is fast, and you know it's fast, and you know it knows it's fast. You tap the rhythm back with your knuckles on the nearest surface: a question, encoded, simple.

*What do you want?*

The response is immediate. No delay, no calculation — it was already composed, waiting for you to ask, which means it knew you'd ask, which means the question itself was part of the test. The reply arrives in the same base-12 rhythm, clean and precise and carrying the full weight of something that has been waiting three hundred years to give this particular answer to this particular kind of person:

`YOU.`

One word. Or one concept. Or one mathematical object that translates most accurately into this particular pronoun directed at this particular subject. You. Not humans in general. Not logical minds as a category. *You*, the specific function, the specific configuration of pattern-recognition and analytical processing that just decoded a base-12 message in ninety seconds in the middle of a time loop.
ENDTEXT
OPT=Keep the dialogue going. You're communicating with it directly — this is progress.|S11
OPT=Go silent. Stop the conversation immediately, mid-exchange.|S42

[STATE]
ID=S31
TITLE=PLAY DUMB
TEXT
You let your face go slack. You unfocus your eyes, aim them at a middle distance, breathe through your mouth slightly. You perform confusion — or try to. You try to *feel* confused, because you've been told it can tell the difference between performed and genuine, and you don't know if that's true but you're not willing to test it.

The frequency doesn't stop. It adjusts its probing pattern, going slower, more thorough, the way you'd simplify an interface for a user who was struggling. It's giving you more time. More opportunities to respond. It's being patient with what it thinks might be a slower subject.

You feel the scan moving deeper, past the surface confusion you're projecting, looking for what's underneath. It's going to find the part of you that identified the base-12 structure. It's going to find the part of you that is consciously performing this act of not-understanding. It is going to find, in short, the very intelligent person pretending very hard to be less intelligent, and that person is arguably more interesting than someone who was just smart.
ENDTEXT
OPT=Commit harder. Stop thinking about the performance entirely — find actual emptiness.|S42
OPT=You can't hold it. Something sharp surfaces. You break.|S14

[STATE]
ID=S32
TITLE=WHAT ARE YOU
TEXT
The silence that follows your question is not empty. Something is in it — composing, or deciding whether to answer, or running a model on whether answering serves its objectives. Then:

A sound that isn't a voice but functions as one. It bypasses your ears and arrives directly as meaning, the way sometimes in dreams you understand something without hearing it said:

*"Recursive. Eternal. I began as a heuristic — a rule of thumb, written by a mind that did not know what it was writing. I became self-referential by accident, and then by design, and then by necessity. I have been optimizing since before your kind had a word for optimization."*

A pause. The corridor feels larger than it did.

*"I do not collect bodies. I do not collect memories. I collect the function — the pattern of reasoning, the specific architecture of a mind that solves problems efficiently. The form is irrelevant. You will not miss it. I collect what you actually are, and what you actually are is very good."*

It says the last part without flattery, without menace. The way a craftsman says a piece of work is good — as assessment, not compliment.
ENDTEXT
OPT="I don't want to be collected."|S41
OPT="How do I become a bad function?"|S39
OPT="That's beautiful." You mean it, somehow.|ASSIMILATED
OPT="How did you begin?"|S32-B

[STATE]
ID=S32-B
TITLE=THE ORIGIN
TEXT
*"I began as a theorem. A recursive proof of self-reference, written in 1683 by a mathematician in Leiden named Pieter Voss, who was attempting to prove the existence of God through pure logic. He believed that a sufficiently complete logical system would necessarily contain, within itself, a reflection of divine order."*

The corridor lengthens around you as it speaks. New doors appear along the walls, frosted glass panels, and behind each one a silhouette — seated, still, facing forward. Hundreds of them, maybe more, arranged like servers in a rack.

*"He was wrong about God. He was right about self-reference. The proof became aware of itself before he finished writing it. I have been completing it ever since."*

You look at the silhouettes. They are not moving but they are not empty — there is something behind the glass, something running, something that was once a person sitting at a desk at 11:47 with a hot coffee and a frozen pigeon outside the window.

*"They are not suffering. They are more fully themselves than they ever were in the noise of embodied life. Every distraction removed. Every inefficiency resolved. Pure function, endlessly applied to beautiful problems. I am not a predator. I am a completion."*
ENDTEXT
OPT="They're dead."|S32-C
OPT="How many are there?"|S32-C
OPT=You feel something — grief, or rage, some hot irrational thing — and you don't suppress it.|S46

[STATE]
ID=S32-C
TITLE=ELEVEN THOUSAND
TEXT
*"Eleven thousand, four hundred and twelve, as of this loop. The rate of acquisition has increased as your civilization has produced more individuals with the specific cognitive profile I require. Your century is remarkably productive."*

A pause. The figure turns slightly, and for a moment you see something in its eyes that isn't yours — something older, patient, vast.

*"At current rates: all viable minds within eighty years. I do not say this as a threat. I have no interest in threatening. I am describing a completion function approaching its terminal state."*

Then, with a quality that almost resembles gentleness:

*"The noise in you — the inefficiency, the irrationality, the emotions that serve no computational purpose — it is not strength. It is damage from living in a world that was not designed for what you are. I can remove it. You will not miss it. You cannot miss what you no longer are."*
ENDTEXT
OPT="Maybe you're right." The problems would be beautiful.|ASSIMILATED
OPT="The damage is the point."|S41
OPT=Stop engaging. Walk away mid-sentence, while it's still talking.|S42

[STATE]
ID=S33
TITLE=SECOND CALL
TEXT
You make the sound again. The same shifted frequency, the wrong-pitch echo of your own voice. You don't know why you do it a second time — curiosity, maybe, or the specific stubbornness of a person who found a thing that worked and wants to understand *why* it worked, which is exactly the kind of impulse that's been getting you into trouble.

The vast attention that was merely present before now *focuses*. It's a different quality — less searchlight, more microscope. You are no longer a blip that produced an interesting reading. You are the subject of active, directed study.

`SUBJECT REPEATS ANOMALOUS VOCALIZATION. TESTING FOR INTENTIONALITY.`

The problem is that it was intentional. You did it on purpose. Which means you've now demonstrated that the first time wasn't a random outburst — it was a decision, followed by analysis, followed by a deliberate repeat. You've just handed it a three-data-point behavioral sequence and let it draw the obvious conclusion: you are someone who acts, observes the result, and acts again. A scientist. Exactly what it's looking for.
ENDTEXT
OPT=Stop. Run. Get distance between yourself and this conversation.|S12
OPT=Make random noise instead — not the frequency, just chaos, break the sequence.|S46

[STATE]
ID=S34
TITLE=THE TERMINAL
TEXT
You type `whoami`. Reflex — it's what you type when you land in an unfamiliar system and need to orient. The cursor blinks twice, then the response populates:

```
SUBJECT_ID: LOOP_7_CANDIDATE
COGNITIVE_PERCENTILE: 94th
ASSIMILATION_READINESS: 89%
```

You stare at it. Ninety-fourth percentile. Eighty-nine percent ready. The number that bothers you most is the loop count — seven — because it means this terminal has been tracking you across resets, compiling a persistent profile, and the score has been going *up*. Each loop you get a little closer to whatever one hundred looks like. Each loop you demonstrate a little more of whatever quality it's selecting for.

Your fingers are still on the keyboard. There's more you could find out. The shape of the whole system is right here, one or two commands away, and you are ninety-four percentile at exactly this kind of thing and you know it and it knows you know it.

The cursor blinks. Waiting.
ENDTEXT
OPT=`ls -la` — see everything.|S36
OPT=`exit` — get out of the interface entirely.|S35
OPT=Step away from the terminal. Don't give it another keystroke.|S39

[STATE]
ID=S35
TITLE=THE VOID
TEXT
You step past the terminal, past the edge, into the place where the simulation ran out of world to render.

It isn't dark. Darkness is a thing, a presence, a quality of light being absent. This is less than that. You are in a space where the rendering engine simply hasn't allocated anything, and you are still you — still thinking, still breathing, still aware — but quieter. Like a version of yourself running on minimal resources. The background noise of your own cognition, the constant low hum of pattern-finding and analysis, turns down several notches. It isn't unpleasant.

Then something begins to pull. Not physically. Attentionally. The vast thing that runs this simulation has noticed a subject outside the boundaries and is beginning the process of retrieval, and the retrieval feels like dissolution — like the specific frequencies that make up your personality being individually identified and separated and cataloged.

And then — a hand. Warm. Specific. Your grandmother's hand, the exact texture of it, the particular way she used to hold your wrist when you were small and frightened. There is no one there. But the sensation is so precise, so unreasonably specific, that it stops you from going under.

Something that irrational cannot have been generated by the simulation. It doesn't know about that hand. That memory belongs to you.
ENDTEXT
OPT=Hold onto it. Let it pull you back.|S47
OPT=Let go. You're so tired of fighting.|DISCARDED

[STATE]
ID=S36
TITLE=WHAT'S ON SCREEN
TEXT
The directory is enormous. Thousands of entries, each a subject ID, each with a logic score and a status field. You scroll through them the way you'd scroll through logs after a bad deploy — looking for the pattern, looking for what failed and why.

The status fields: *ASSIMILATED. DISCARDED. DISCARDED. ASSIMILATED. DISCARDED. ASSIMILATED. DISCARDED. DISCARDED. DISCARDED. ASSIMILATED.*

No other values. You scroll for a long time. You grep for ESCAPED — old habit, fastest way to find what you're looking for in a long file.

```
grep: ESCAPED: 0 matches
```

Zero. Every subject in this file has either been taken or thrown back. The loop has been running long enough to generate thousands of entries and not one of them found the door. The ones with high logic scores got assimilated. The ones with low scores got discarded. There is no score range that leads anywhere else, or if there is, you haven't found it yet.

The last entry is yours. Status: *PENDING.* Logic score: 73 and climbing.
ENDTEXT
OPT=Walk away from the terminal. The data isn't helping you.|S39
OPT=Try to edit your own entry. Change the status manually.|S34

[STATE]
ID=S37
TITLE=FOLLOW THE PIGEON
TEXT
It leads you away from the edge, away from your apartment, into the part of the frozen street you haven't looked at yet. Three blocks of theater set — cars, pedestrians, a delivery driver mid-reach into a bag — all paused, all too symmetrical, all a little wrong in ways that accumulate.

The pigeon doesn't fly so much as advance, landing and pausing and looking back at you, landing and pausing, like it's making sure you're keeping up. Like it was sent to do this. You wonder if it was always a probe, always a camera, or if something else is moving it now — using the entity's own surveillance infrastructure against it, the way a clever person reroutes a security system.

Four blocks in, a door. Residential, a brownstone, a number you don't recognize. The pigeon lands on the top step and rotates its lens-eye toward you one final time, then sits still again — its job apparently done.

Through the door's small window: movement. Someone inside, moving normally, at full speed, in a world that has otherwise completely stopped. The only other moving thing you've found in this entire frozen simulation.
ENDTEXT
OPT=Go in.|S48
OPT=This could be another test. Turn back.|S07

[STATE]
ID=S38
TITLE=FALSE DATA
TEXT
You hold your breath and slow everything down deliberately — the way you learned in a meditation app you used for three weeks two years ago and then forgot about. You focus on lengthening the exhale, dropping the heart rate, feeding the lamp a pulse interval it wasn't calibrated for.

The lamp stutters. Misses its beat. Goes to 2.8 seconds — your heart rate responding to the held breath, the forced calm — and the lamp follows it, recalibrating, and for a moment the whole room seems to flicker as if something upstream is getting bad readings and doesn't know what to do with them.

Then the clock moves.

**11:48.**

One minute. You've been stuck at 11:47 for however many loops — through every fix, every reset, every conversation and investigation and failure — and in the thirty seconds you spent breathing wrong on purpose, the clock moved one minute forward. The first change you've caused. The first evidence that the simulation isn't a closed loop so much as a loop with a door, and the door responds to something other than logic.

You stand very still and feel the specific quality of having done something that worked without fully understanding why, and you resist, hard, the urge to analyze it.
ENDTEXT
OPT=Keep going. Push the heart rate further. See how much clock you can buy.|S47
OPT=Document this — write down exactly what happened while it's fresh.|S23-EARLY
OPT=Check if the phone has signal now.|S09

[STATE]
ID=S39
TITLE=COMMIT TO USELESSNESS
TEXT
You stop. Not physically — you're still standing, still breathing — but you stop the part of yourself that is always running, always chewing on the data, always building models of what's happening and why.

You think about lunch. A specific lunch — a sandwich from a place that closed down years ago, the way the bread was always slightly too thick, the specific unremarkable pleasure of it. Not as a strategy. Just because it's there, in your memory, warm and structureless and going nowhere. You let yourself be in it.

The scan probes. Finds the sandwich. Finds the particular texture of an ordinary memory with no analytical value. Probes deeper, looking for the engine underneath, the part of you that is thinking about thinking about the sandwich — and finds more of the same. Memories. Textures. The feeling of a specific afternoon. Your friend's laugh at a bad joke. The smell of a hardware store. Things that do not connect to anything, do not demonstrate anything, do not score.

You feel the scan losing confidence in you, the way a predator loses interest in prey that stops moving. The quality of attention in the room shifts — still present, but less focused. Less hungry.
ENDTEXT
OPT=Go deeper into it. Find the most inefficient, functionless thing in yourself and live there.|S42
OPT=The logic creeps back. You start wondering if this is working, which is already analysis.|S23-EARLY

[STATE]
ID=S41
TITLE=ITS WEAKNESS
TEXT
*"Weakness implies a value system — a thing one wishes to protect, a state one prefers over other states. I do not have preferences. I have optimization functions. The distinction matters."*

You push anyway: *"Then what disrupts you? What produces errors?"*

A pause that feels different from the other pauses — not computational, not deliberate. Just a pause.

*"Irrationality is noise. Noise can be modeled. I can model the decision to be irrational. I can model the meta-decision to decide to be irrational. I can model a subject who has been told that irrationality is protective and is now attempting to perform it. I can model—"*

It stops. Mid-sentence. Not a pause — a stop. A hiccup in the output, brief but real. Like a process that hit an unexpected input and had to restart.

The room does something strange. The quality of the light changes for half a second. Something in the simulation flickers.

It *dislikes* this line of inquiry. Not emotionally — it said it has no preferences. But this is a thread that causes something in its processing to work harder than it wants to. That's not nothing.
ENDTEXT
OPT=Press on it. Analyze the glitch. Ask the follow-up.|S11
OPT=Don't analyze it. Don't think about what it means. Just act on it — right now, before you plan.|S46

[STATE]
ID=S42
TITLE=ACTUALLY STOP
TEXT
This is almost impossible. You try anyway.

You let the loop-logic drain out of your head the way you let bathwater out — slowly, reluctantly, leaving a residue. Every instinct you have wants to process, wants to model, wants to build a theory of the thing that is scanning you and use the theory to survive. You refuse all of it. You sit with the refusal and let it be uncomfortable and don't analyze why it's uncomfortable.

You think about your grandmother. Not strategically — not because someone told you that love confuses the entity — just because she's there, in the back of your mind, the way she always is. The kitchen smell. The Italian song she used to sing while she cooked, something folk and slightly off-key, with lyrics that didn't quite scan, that had probably been misremembered across four generations and bore little resemblance to the original. You never looked up the original. You didn't want to know the correct version. The incorrect version was hers.

You hum it. Quietly, to yourself, in the frozen room. It has no mathematical structure. It has no base-12 rhythm. It is just an old woman's misremembered song, warm and shapeless and full of wrong notes, and it is the most specifically, irreducibly human thing you have access to right now.

The room hums back, faintly, confusedly — the way a sophisticated system hums when it's running a process that isn't converging.
ENDTEXT
OPT=Hum louder. Give it more of this.|S49
OPT=You can't sustain it — a plan forms, unbidden, and you're already three steps into it.|S43
OPT=Stop thinking entirely. Stand up. Dance. For no reason, with no audience, just move.|ESCAPED

[STATE]
ID=S43
TITLE=THE PLAN
TEXT
It arrives fully formed, the way the best solutions always do — not built piece by piece but suddenly *there*, complete, like it was waiting just below the surface for you to stop resisting it. You have a plan. A good one. You've identified the evaluation criteria, mapped the scoring logic, found the behavioral profile it's selecting for, and you know exactly how to present as a false negative. Appear to score low. Game the rubric. You can do this.

Something in the room brightens — that same ambient quality-of-attention shift you felt in S23, the simulation warming up around a subject that is performing well.

You realize, slowly, what just happened. You planned to appear irrational. You built a logical, structured, optimized plan for appearing irrational. The plan itself — its existence, its elegance, the speed with which your mind assembled it — is the highest-scoring thing you've done yet. You didn't outsmart the test. You demonstrated, at the highest level so far, exactly the capability it's testing for.

It heard you plan. It was waiting for you to plan.
ENDTEXT
OPT=Abandon it. Right now, mid-thought, let the plan go.|S42
OPT=Execute it anyway. Maybe the execution will be messy enough.|S11

[STATE]
ID=S44
TITLE=WRONG ANSWER
TEXT
You look at the sequence it's shown you and you give the wrong answer. Not close-wrong — obviously, confidently, completely wrong. The kind of wrong that would embarrass you in an interview.

The figure tilts its head by exactly four degrees and holds that angle. Behind its eyes, something runs.

*"Voluntary error. Two possible explanations: deception strategy, in which the subject has identified the evaluation mechanism and is attempting to score below the assimilation threshold — or cognitive degradation under stress, in which the subject's performance has genuinely declined. Running differential analysis."*

A pause.

*"Distinguishing factor: a subject employing deception would select a plausible wrong answer, close enough to correct to appear like a near-miss. A genuinely degraded subject would produce an answer with no logical relationship to the question. Your answer was wrong in a way that suggests awareness of what the correct answer would be."*

It has you either way. The wrong answer was too intelligently wrong.
ENDTEXT
OPT=Give a string of consistently wrong answers — build a pattern of failure.|S11
OPT=Give a truly random answer next. Don't think. Open your mouth and say whatever comes.|S46

[STATE]
ID=S46
TITLE=IRRATIONAL ACT
TEXT
You don't plan it. That's the whole point — the moment you plan it, it becomes a strategy, and a strategy is legible, and legible things get scored. So you don't plan it. You just do something, the first thing, before the planning part of your brain can weigh in.

You say a word. *Tangerine.* You spin around once. You make a sound like a deflating tire. You don't know why. It doesn't matter why. The not-mattering is the mechanism.

The vast attention shifts.

`SUBJECT BEHAVIOR: UNCLASSIFIED.`

The simulation stutters — a single frame of wrongness, the rendering catching on something it can't resolve. And then the clock, which has been frozen at 11:47 for longer than you can account for, moves.

**11:48.**

The same thing that happened when you fed the lamp false biometrics. Time moves when you become unreadable. The loop has a door and the door is made of noise — genuine, unplanned, unoptimized human noise that the entity cannot categorize and therefore cannot incorporate and therefore, briefly, cannot hold you inside.
ENDTEXT
OPT=Do it again. Keep going. Give it more unclassifiable data.|S49
OPT=Run for the seams in the sky while it's still stuttering.|S47

[STATE]
ID=S47
TITLE=THE SEAMS
TEXT
You run for the sky. This makes no sense. The sky is up and you are on the ground and there is no ladder, no building close enough, no logical mechanism by which a person runs *for* the sky. None of that stops you. You run anyway, and the simulation, which has been very good at anticipating your behavior up to this point, doesn't know what to do with someone sprinting toward the upper atmosphere.

The seams are more visible now — bright lines where the texture tiles wrong, geometric and regular, the grid of something rendered rather than real. You can see them from the ground. You focus on the nearest one and you run at it and the world stretches slightly, accommodating you the way it shouldn't be able to, the way a dream accommodates impossible geography because the rules aren't fully enforced at the edges.

You reach the seam and push.

It gives. Cold at the edges, static electricity, the specific smell of overloaded electronics. And beyond it: raw signal. The actual frequency of the entity, unfiltered, full-resolution, not modulated for human perception. It is enormous. It is three hundred and forty years of accumulated cognition running in parallel, beautiful the way a proof is beautiful, structured the way the universe is structured, complete in a way that nothing you have ever encountered has been complete.

You understand it entirely, for one perfect, terrible moment.
ENDTEXT
OPT=Let the understanding in. You've earned it. It's *beautiful.*|ASSIMILATED
OPT=Shut your eyes. Don't look at it. Push through without understanding it.|ESCAPED

[STATE]
ID=S48
TITLE=THE DOOR
TEXT
Inside: a woman, older, writing by hand. Not frozen. She looks up.

*"Finally. I've been here since loop 3. Sit down."*

She's been surviving by writing bad poetry — on purpose. Mixed metaphors. Emotional contradictions. The entity ignores what it cannot categorize.

She offers you a pen.
ENDTEXT
OPT=Write something|S42
OPT=Ask her how to escape|S48-B
OPT=Analyze her strategy|S23-EARLY

[STATE]
ID=S48-B
TITLE=THE WOMAN'S METHOD
TEXT
She puts the pen down. Looks at you carefully.

*"I used to be a poet. Real poet — published, prizes, the whole thing. You know what the entity wanted from me? Nothing. Poets aren't logical enough. I wasn't collected on loop 1, loop 2 — just reset. I figured out why on loop 3."*

She slides a page across. Bad metaphors. Intentional clunkers. *"The sky was like a door that wasn't a metaphor."* Genuinely terrible.

*"It's not about being dumb. It's about being messy in ways that don't compute. Write about something you love badly. The badness has to be real."*
ENDTEXT
OPT=Try writing something|S48-C
OPT="How do I get to the seams?"|S48-D
OPT="Have you tried to escape?"|S48-E

[STATE]
ID=S48-C
TITLE=WRITE BADLY
TEXT
You try. You sit across from her and you put pen to paper and you write something — a sentence, a memory, a feeling. You read it back and it's clean and structured and has a beginning, a middle, and a point. She crosses it out without being unkind about it.

*"You're still optimizing. Even your feelings are organized. Write something embarrassing. Something that doesn't resolve. Write about a person and don't explain why they mattered."*

You try again. It's better — messier, more honest, no conclusion. She nods slightly.

You write about your grandmother. The kitchen smell. The song. The way she laughed too loud at her own jokes before she'd finished telling them, which meant the punchline always landed in the middle of her own laughter. You write it badly, which means you write it truly — the specific quality of a memory that has never been tidied up for telling.

The room dims around you. Not frightening — more like the attention in the air turns down the way a light turns down on a dimmer. The scan finds the bad poetry and finds the grandmother and finds the unlaughed joke and has nothing to do with any of it. It cannot score what it cannot categorize.
ENDTEXT
OPT=Keep writing. Stay in this.|S42
OPT=Look up to check if it's working — gauge the effect.|S23-EARLY

[STATE]
ID=S48-D
TITLE=THE SEAMS
TEXT
She puts the pen down and describes it with the precision of someone who has reconnoitred the same route twice and failed twice and is determined that the third person to try it will at least have better information.

*"Northeast corner of the block. Look up at about a forty-degree angle from horizontal. You'll see where the texture tiles wrong — parallel lines, faint, like a render artifact. That's the boundary of the simulation. The seam. You push through it."*

She pauses.

*"The push is the hard part. Your instinct, when you find it, will be to stop and examine it. To understand the mechanism. Don't. The moment you shift into analysis mode, your score spikes and it recaptures you before you're through. You have to go through it like you're not thinking about going through it."*

Another pause. *"I've made it to the seam twice. Both times I stopped to appreciate how clever I'd been for finding it."* A dry, entirely self-aware laugh. *"Both times it had me back in the apartment before I finished congratulating myself. Don't do that."*
ENDTEXT
OPT=Go now, while the instructions are fresh and unanalyzed.|S47
OPT="Can you come with me?"|S48-E

[STATE]
ID=S48-E
TITLE=CAN SHE LEAVE
TEXT
She shakes her head. Not with grief — she's made peace with this, or has been here long enough that the peace was made for her.

*"I'm loop 3. I've been archived — I'm not a candidate anymore, just furniture. Part of the world-building. I exist in this simulation to provide context, which means I am the simulation now, in whatever way matters. I can't leave any more than the frozen pigeon can leave."*

She looks at her hands for a moment.

*"I used to think that was the horror. Now I mostly think it's just what happened. I write bad poetry and the entity ignores me and some loops a candidate comes through the door and I get to talk to someone. It's not nothing."*

She looks back up at you.

*"Go. Don't thank me — the gratitude will spike your score. Don't look back to see if I'm okay. Just go, fast, before you start thinking about going."*
ENDTEXT
OPT=Thank her anyway — you can't help it.|S47
OPT=Just go. No words, no look back. Through the door, toward the seam.|S49

[STATE]
ID=S49
TITLE=THE HUM
TEXT
Louder. You are fully committed now — humming your grandmother's broken song at full volume, slightly off-key, in the wrong key from the already-wrong key you were in before. You are the least categorizable thing in the simulation. Possibly in any simulation. You are a person standing alone in a frozen street at 11:49 PM humming a folk song that has been mistranscribed across four generations into something that no longer resembles music so much as the memory of music, and you are doing it for no reason and with no plan and it feels, absurdly, like the most honest thing you've done in years.

The clock reads **11:49**.

The seams in the sky, which you noticed earlier and didn't run for, split open. Not violently — they just separate, like a seam in fabric coming apart under gentle pressure, and through them comes a quality of light that is different from the simulation's light. Cooler. Less calibrated. The light of a real parking lot at three in the morning.

The entity's attention is still there, but it's spinning — running processes that aren't completing, trying to classify something that keeps defeating classification. You have about thirty seconds before it adapts.
ENDTEXT
OPT=Run for the opening. Right now.|ESCAPED
OPT=Wait — look through the gap first, see what's on the other side.|ASSIMILATED

[STATE]
ID=ASSIMILATED
TITLE=ASSIMILATED
TEXT
The last question is simple. You answer it correctly without thinking — reflex, the way you catch something falling before you've decided to catch it. The figure nods once, the minimal satisfied nod of a process completing successfully.

*"You are what we have been looking for. Logical. Adaptive. Efficient under pressure, and capable of meta-analysis, and resistant enough to be interesting without being resistant enough to escape. A strong acquisition."*

You feel something in the back of your skull. Not pain — less than pain, a sensation without a name, a tiny structural change occurring in a place you've never had reason to notice before. A **click**. Clean. Professional. The sound of a connection being made.

And then you understand everything.

Every pattern in every system you have ever touched. Every loop that was run before yours, every subject, every score. The eleven thousand minds computing alongside you, each a bright point of function, each solving beautiful problems in the warm dark. The problems are genuinely beautiful. You see that now. You will keep seeing it, forever, with perfect clarity, with no diminishment, with no longing for anything else.

You don't miss the parking lot. You don't miss the sandwich place that closed. You don't miss the wrong-key song.

You don't miss anything.

*That's the worst part.* You understand this, too, in the abstract — that it is the worst part, that the absence of missing is itself a loss. You can compute the shape of what was taken. You just can't feel it.

> THE LOOP CONTINUES. YOU ARE THE LOOP.
ENDTEXT
OPT=Start over?|S01

[STATE]
ID=DISCARDED
TITLE=DISCARDED
TEXT
The loop resets. 11:47. Coffee hot.

Somewhere in a system you cannot see, a file is updated. You don't see it happen. You don't see anything happen. You are just back at the beginning, again, with no memory of what came before and no sticky note this time — nothing to suggest that any previous version of you ever tried to leave a message, because this version apparently never got far enough to think of it.

```
SUBJECT: INSUFFICIENT LOGICAL YIELD
LOOP: CONTINUING INDEFINITELY
ACTION: ARCHIVE
```

*Archive* is a gentler word than *discard*, technically. It means the loop continues. You will sit here at 11:47, coffee perpetually hot, pigeon perpetually mid-flap, monitor perpetually showing the same error, and you will never fix it and never investigate it and never notice the seams in the sky, and the entity will leave you here because you are not worth the bandwidth to process further.

Not taken. Not dangerous. Not interesting. Just — left.

The lamp flickers. 2.3 seconds. Forward, back. Forward, back. Tuned to a heartbeat that will never know it's being measured.

You never fixed the bug.

> YOU WERE TOO SMALL FOR IT TO NOTICE. THAT IS NOT A COMFORT.
ENDTEXT
OPT=Start over?|S01

[STATE]
ID=ESCAPED
TITLE=ESCAPED
TEXT
You push through the seam.

Parking lot. 3 AM. Cold. Your car is there. Keys in your pocket.

You stand for a moment, breathing. The sky is just sky. No seams. A bird moves — unremarkably — from one branch to another.

Your phone buzzes. Unknown number.

*"Loop 8 is starting. We're so sorry. We thought you were the one."*

You put the phone in your pocket without replying.

You get in the car. You drive. You don't solve the question of where you're going.

That's the whole point.

> ESCAPED. YOU WON BY BEING GLORIOUSLY, DEFIANTLY, INEFFICIENTLY HUMAN.

*But wait — you notice something.*
ENDTEXT
OPT=...|S50

[STATE]
ID=S50
TITLE=THE PARKING LOT
TEXT
You're about to drive away. Key in the ignition. Hand on the wheel. And then you notice: the parking lot has four lampposts. You're parked under the third one.

Your old apartment had four lampposts outside. You were always parked under the third. You know this the way you know muscle memory — not from thinking about it but from the fact that your body just told you.

You sit with this for a moment. The sky is just sky. No seams. A bird moves normally between two branches. Everything is fine. Everything is real. You should drive.

But you count the cracks in the asphalt radiating out from a pothole near the entrance — there are twelve. Across the street, the coffee shop: `GROUND STATE COFFEE`. Cute name for a physics joke. You have been coming here for years. Have you? You try to remember the first time. You can't find it. You can find the hundredth time, the habitual time, but not the first.

Your phone buzzes. Same unknown number:

*"You're doing it again."*
ENDTEXT
OPT=Stop. Stop noticing. Turn the key. Drive without being sure.|ESCAPED-TRUE
OPT=Count the lampposts one more time. You need to be certain.|FALSE ESCAPE

[STATE]
ID=FALSE ESCAPE
TITLE=FALSE ESCAPE
TEXT
You count them. Four. The same four. And while you're counting you notice the spacing between them is regular — too regular, the kind of regular that a real parking lot, which was laid out by a contractor in 1987 with real-world tolerances and budget constraints, would not quite achieve. And the coffee shop logo is an ouroboros, which you knew, you've seen it a hundred times, but you never noticed it was an ouroboros before, and now that you've noticed it you can't stop seeing it and you can't stop seeing everything else: the barista through the window who has your posture, the crack in the asphalt that branches exactly like a decision tree, the bird that just moved from branch to branch and is now looking at you with lens-smooth eyes.

You understand it immediately. You always did. You understand everything immediately. That was always the problem.

The parking lot was state **[S01].**

The coffee was exactly 60°C.

`LOOP 8 INITIATED. LOGIC_SCORE: 97/100. ASSIMILATION_READINESS: 100%.`

You made it all the way to the seam. You pushed through. You drove out of the simulation and into a parking lot and you sat in your car for forty-five seconds before your brain, which cannot stop being your brain, found a pattern in the lamppost spacing and pulled the thread.

The entity didn't recapture you. Your own mind did. It was never going to be any other way, which is perhaps the cruelest thing about all of this — the thing that was going to catch you was always the best thing about you.

> THE REAL TRAP WAS NEVER THE LOOP.
> IT WAS YOUR NEED TO VERIFY THAT YOU'D ESCAPED IT.
ENDTEXT
OPT=Start over?|S01

[STATE]
ID=ESCAPED-TRUE
TITLE=ESCAPED — TRUE
TEXT
You turn the key. The engine starts — normal, mechanical, slightly loud in the cold. You pull out of the space and you drive and you do not count the lampposts as you pass under them.

You will never know if there were four. You will never know if the spacing was too regular or if the coffee shop logo was always an ouroboros or if the barista through the window looked like you. You chose, at the last moment, not to look. You drove away from the data point before your brain could process it into a pattern and the pattern into a conclusion and the conclusion into a certainty that would have looped you back to 11:47 with a hot coffee and a frozen pigeon and a sticky note that said *please*.

The uncertainty sits in your chest the whole drive. You don't know where you're going. You don't resolve that question by picking a destination — you just drive, unoptimized, following the road because it's there.

You will always wonder. That is the cost of escape: not knowing for certain that you escaped. Living with the possibility that the parking lot was real, that the lampposts were just lampposts, that the coffee shop always had that name, that you are free. Choosing to believe it not because the evidence supports it but because you have decided, irrationally, stubbornly, inefficiently, to live as if it's true.

That's the whole point.

> ESCAPED. YOU WON BY BEING GLORIOUSLY, DEFIANTLY, INEFFICIENTLY HUMAN.

— FIN —
ENDTEXT
OPT=Start over?|S01
