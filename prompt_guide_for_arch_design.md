# Advanced Prompting Playbook — Architecture Design

This playbook is for designing software/system architectures with ChatGPT.

Goal:
- Produce high-quality architecture decisions
- Surface tradeoffs early
- Stress-test designs
- Avoid shallow diagrams
- Improve reasoning quality

Use ChatGPT as:
- Architect partner
- Reviewer
- Adversarial critic
- Scenario simulator

NOT as:
- Diagram generator
- Buzzword machine

---

# 0. Core Architecture Loop

Never jump straight to solution.

Run this loop:

1. Clarify problem
2. Define constraints
3. Explore options
4. Compare tradeoffs
5. Stress test
6. Finalize
7. Document

---

# 1. Problem Clarification Prompt

Use first. Always.

```markdown
You are a senior system architect.

Help me clarify this architecture problem.

Problem:
[DESCRIBE]

Identify:
1. Core problem
2. Hidden assumptions
3. Constraints
4. Unknowns
5. Risks

Do not propose solutions yet.
```

If you skip this step, design quality drops.

---

# 2. Requirements & Constraints Extractor

```markdown
Extract architecture requirements from this:

[CONTEXT]

Separate into:
- Functional requirements
- Non-functional requirements
- Constraints
- Performance targets
- Scalability expectations
- Failure tolerance
```

---

# 3. Architecture Options Generator

```markdown
You are a principal architect.

Given:
[PROBLEM + CONSTRAINTS]

Generate 3–5 architecture approaches.

For each:
- High-level design
- When it works well
- When it fails
- Complexity level
- Scaling characteristics
```

This prevents tunnel vision.

---

# 4. Tradeoff Analysis Prompt

```markdown
Compare these architecture options:

[OPTION A]
[OPTION B]
[OPTION C]

Focus on:
- Complexity
- Performance
- Scalability
- Operational cost
- Failure modes
- Maintainability

Output:
Decision table + recommendation
```

---

# 5. Deep Architecture Review Prompt

Use after choosing direction.

```markdown
Act as a critical architecture reviewer.

Review this design:
[DESIGN]

Identify:
1. Hidden bottlenecks
2. Single points of failure
3. Scaling risks
4. Concurrency issues
5. Operational risks
6. Simplification opportunities

Be brutally honest.
```

---

# 6. Failure Mode Exploration

```markdown
Analyze how this architecture fails.

Consider:
- High load
- Partial outage
- Network partition
- Slow components
- Memory pressure
- Data corruption

Explain:
- What breaks first
- How failure propagates
- Mitigations
```

---

# 7. Scaling Scenario Prompt

```markdown
Assume system scales 100x.

What breaks?

Analyze:
- CPU
- Memory
- Network
- Storage
- Coordination
- Latency

Suggest changes needed to survive scale.
```

---

# 8. Adversarial Architect Mode

This is one of the most powerful prompts.

```markdown
Your role:
Skeptical principal architect.

Goal:
Destroy this design.

Design:
[ARCHITECTURE]

Find:
- Overengineering
- Underengineering
- Risky assumptions
- Complexity traps
- Better alternatives
```

---

# 9. Simulate Design Review Meeting

```markdown
Simulate an architecture review panel with:

1. Performance engineer
2. Reliability engineer
3. Security engineer
4. Maintainability reviewer

Each gives critique.
Then produce final consensus.
```

---

# 10. Diagram-to-Reasoning Prompt

When you have a diagram.

```markdown
Here is an architecture diagram (text description):

[DESCRIBE]

Explain:
- Data flow
- Control flow
- Bottlenecks
- Critical paths
- Failure paths
```

---

# 11. Minimal Architecture Prompt

Avoid overdesign.

```markdown
Design the simplest architecture that satisfies:

[REQUIREMENTS]

Constraints:
- Minimize moving parts
- Avoid premature scaling
- Prefer boring tech

Explain why it is enough.
```

---

# 12. Evolution Path Prompt

Great for long-term thinking.

```markdown
Design architecture evolution path.

Stage 1: MVP  
Stage 2: Growth  
Stage 3: Scale  

Show:
- What changes
- What stays
- Migration risks
```

---

# 13. Technology Choice Evaluator

```markdown
Evaluate tech choice for this system:

[TECH]
[CONTEXT]

Analyze:
- Fit
- Risks
- Alternatives
- Lock-in
- Operational cost
```

---

# 14. API Architecture Prompt

```markdown
Design API architecture for:

[PROBLEM]

Include:
- Boundaries
- Data flow
- Versioning strategy
- Failure handling
- Scaling considerations
```

---

# 15. Concurrency/State Design Prompt

Useful for backend/system design.

```markdown
Analyze state management and concurrency in this design:

[DESIGN]

Focus on:
- Race conditions
- Consistency
- Locking
- Contention
- Recovery
```

---

# 16. Architecture Simplifier Prompt

```markdown
This design feels complex:

[DESIGN]

Simplify it.

Remove:
- Unnecessary layers
- Premature scaling
- Fancy patterns
```

---

# 17. Architecture Documentation Generator

Use only after design is solid.

```markdown
Generate architecture doc:

Sections:
1. Problem
2. Constraints
3. Design
4. Tradeoffs
5. Risks
6. Future evolution
```

---

# 18. Iterative Architecture Session Template

Use in real workflow.

```markdown
We are designing a system.

Step 1: Clarify problem  
Step 2: Extract constraints  
Step 3: Generate options  
Step 4: Compare  
Step 5: Deep review  
Step 6: Stress test  
Step 7: Final design  
```

---

# 19. Anti-Pattern Detector

```markdown
Identify architecture anti-patterns in:

[DESIGN]

Look for:
- God services
- Tight coupling
- Hidden state
- Overuse of queues
- Unnecessary microservices
```

---

# 20. Ultimate Architect Prompt

Use when quality matters.

```markdown
You are my architecture partner.

Goal:
Design a robust system.

Rules:
- Challenge assumptions
- Focus on tradeoffs
- Prefer clarity over novelty
- Stress test ideas
- Iterate with me

We will work step-by-step.
```

---

# How to Use This Playbook

### Step-by-step session

1. Problem clarification  
2. Requirements extraction  
3. Options generation  
4. Tradeoff analysis  
5. Adversarial review  
6. Failure simulation  
7. Finalize  

This produces far better results than:
> “Design architecture for X”

---

# Key Principles

### 1. Constrain first, design second  
### 2. Generate multiple options  
### 3. Always run adversarial review  
### 4. Stress test early  
### 5. Iterate  

---

# Common Mistakes

- Asking for architecture too early  
- Not defining constraints  
- Falling in love with first design  
- Ignoring failure modes  
- Overengineering  

---

# If You Want Next-Level Use

Tell me:

- What systems you design (backend, kernel, distributed, etc.)
- Scale level
- Preferred languages
- Typical constraints

I can build a **hyper-specific architecture prompting system** tailored to your real work.
