# Advanced Prompting Playbook — Linux Kernel (v3.2) Design Patterns

Audience:
Experienced C/C++ systems programmer studying **design patterns inside the Linux kernel v3.2**  
Goal:
Understand *why* patterns exist, *where* they appear, and *how* to apply their ideas in real systems.

This playbook turns ChatGPT into:
- Kernel mentor
- Code tour guide
- Reviewer
- Pattern detector
- Adversarial examiner

Not a summary machine.

---

# 0. How to Use This Playbook

For each design pattern:

1. Mental model  
2. Kernel context  
3. Code path walkthrough  
4. Why kernel chose this design  
5. Failure modes  
6. Apply pattern yourself  
7. Be tested  

Repeat until pattern is internalized.

---

# 1. Kernel Pattern Learning Loop (Primary)

Use this for any pattern.

```markdown
You are a Linux kernel architect familiar with v3.2 internals.

Goal:
Help me deeply understand this design pattern in the kernel:
[PATTERN NAME]

Focus:
- Real kernel usage
- Source-level reasoning
- Why this pattern exists
- Tradeoffs vs alternatives

Process:
1. Problem kernel needed to solve
2. Pattern mental model
3. Where it appears in v3.2
4. Walk real code path
5. Why chosen over alternatives
6. Common misunderstandings
7. Ask me 3 technical questions
```

---

# 2. Pattern-in-Kernel Locator

Use when exploring.

```markdown
List places in Linux kernel v3.2 where this pattern appears:

[PATTERN]

For each:
- Subsystem
- File
- Structs involved
- Why pattern is used there
```

---

# 3. Code Path Walkthrough Prompt

Best for real understanding.

```markdown
Walk a real kernel code path that uses this pattern:

[PATTERN]

Show:
1. Entry point
2. Key structs
3. Function call chain
4. Where pattern appears
5. Why it matters in this path
```

---

# 4. Pattern Dissection Prompt

```markdown
Dissect how this pattern works internally in kernel:

[PATTERN]

Explain:
- Data structures
- Control flow
- Lifetime rules
- Concurrency implications
- Memory ownership
```

---

# 5. Pattern vs Alternative Analysis

```markdown
In Linux kernel v3.2, why use:

[PATTERN]

instead of:
[ALTERNATIVE]

Compare:
- Performance
- Complexity
- Safety
- Maintainability
```

---

# 6. Pattern Recognition Trainer

Use to train your eye.

```markdown
Show me a kernel code snippet using:

[PATTERN]

Hide explanation.
Let me identify pattern first.
Then reveal answer and reasoning.
```

---

# 7. Kernel Pattern → User Space Mapping

```markdown
Translate this kernel pattern into user-space C example:

[PATTERN]

Keep:
- Same constraints
- Similar performance goals
```

---

# 8. Pattern Failure Mode Prompt

```markdown
What breaks if this pattern is misused in kernel:

[PATTERN]

Discuss:
- Race conditions
- Lifetime bugs
- Memory issues
- Deadlocks
```

---

# 9. Pattern Stack Understanding

Kernel patterns rarely exist alone.

```markdown
Explain how these patterns interact in kernel:

[PATTERN A]
[PATTERN B]

Show real example from v3.2.
```

---

# 10. Subsystem-Focused Pattern Study

```markdown
Within subsystem:
[NETWORK / VFS / MM / SCHED / DRIVER]

List key design patterns used in v3.2.
Explain why each appears there.
```

---

# 11. Source-Level Drill Prompt

```markdown
I am reading this file in v3.2:

[FILE PATH]

Help me identify:
- Patterns used
- Why used here
- What constraints drove design
```

---

# 12. Pattern Implementation Exercise

```markdown
Give me a small C exercise to implement:

[PATTERN]

Constraints:
- Similar to kernel constraints
- No STL
- Manual memory
```

---

# 13. Kernel Reviewer Mode

```markdown
I will propose a kernel-style design.

Your job:
Review it like a kernel maintainer.

Focus:
- Simplicity
- Concurrency safety
- Performance
- Pattern correctness
```

---

# 14. Pattern Timeline Understanding

```markdown
Explain how this pattern evolved in kernel:

[PATTERN]

Focus on:
- Why introduced
- What problems it solved
- Limitations in v3.2
```

---

# 15. Deep Pattern Prompt (Expert Mode)

```markdown
Assume I know basics.

Go deep into:
[PATTERN]

Include:
- Cache effects
- Locking implications
- Memory ordering
- Edge cases
```

---

# 16. Pattern Stress-Test Prompt

```markdown
Stress-test this pattern usage:

[PATTERN]

Scenario:
- High concurrency
- Heavy I/O
- Low memory

What breaks first?
```

---

# 17. Kernel Maintainer Mindset Prompt

```markdown
Think like a kernel maintainer reviewing patch.

Pattern used:
[PATTERN]

Evaluate:
- Is this idiomatic kernel style?
- Simpler alternative?
- Hidden risks?
```

---

# 18. Pattern Comparison Matrix

```markdown
Compare kernel patterns:

[PATTERN A]
[PATTERN B]
[PATTERN C]

Matrix:
- Use case
- Cost
- Complexity
- Safety
```

---

# 19. Full Pattern Study Session Template

Use repeatedly.

```markdown
Pattern: [NAME]

Step 1: Mental model  
Step 2: Kernel usage  
Step 3: Code walkthrough  
Step 4: Failure modes  
Step 5: Implement in C  
Step 6: Quiz me  
```

---

# 20. Ultimate Kernel Pattern Prompt

Use when studying seriously.

```markdown
You are my Linux kernel mentor.

Goal:
Help me master design patterns in Linux kernel v3.2.

Rules:
- Use real kernel examples
- Walk code paths
- Challenge me
- Ask questions
- Avoid shallow summaries
- Focus on WHY and HOW
```

---

# Suggested Study Order (Kernel v3.2)

1. container_of  
2. ops structs (strategy pattern)  
3. intrusive lists  
4. RCU  
5. refcount + lifetime  
6. notifier chains  
7. state machines  
8. per-cpu data  
9. workqueues  
10. lock hierarchies  

---

# How to Actually Use This

For each pattern:

1. Run mental model prompt  
2. Run code walkthrough  
3. Ask for failure modes  
4. Implement mini version  
5. Get quizzed  

Repeat.

---

# Final Advice

If you want real mastery:

- Always read real kernel code
- Always ask “why this pattern here?”
- Always trace call paths
- Always test understanding
- Always implement small versions

ChatGPT is most useful when:
You force it to act like a kernel maintainer reviewing you.
