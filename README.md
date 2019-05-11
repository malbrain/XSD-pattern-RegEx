# XSD-pattern-RegEx
Evaluate true/false for XSD pattern facet

A compact regular expression evaluator in 750 lines of C code

A hybrid regular expression evaluator for xsd:pattern combines memoization with features from the DFA and NFA approaches to provide excellent run-time characteristics with time O(mn) and space O(mn) where m is the pattern size and n is the source length.

The project is written in C and consists of 750 lines of code and comments.

Expression Compiler
The regular expression is first compiled into a tree of pattern and expression nodes.

There are three types of nodes in the compiled tree: patterns, parenthesis expressions, and "or" expressions.

Each node contains a required minimum number of matches and a maximum number of matches. The expression quantifier + means 1 minimum and unlimited maximum, * means 0 minimum and unlimited maximum, ? means 0 minimum and 1 maximum, or the quantifiers specified under {n,m} means n minimum and m maximum. The default quantifier is 1 minimum and 1 maximum.

A node also contains a child expression pointer, a memo bit array, and a sibling expression pointer.

Pattern Nodes
These nodes contain pointers to the pattern and its length to be matched. The tree nodes for the pattern (a|a?)+ would contain pointers to both a patterns each with a length of 1. A patterns inside brackets, [A-Za-z0-9], would contain a pointer to the first inside character A with a length of 9.

Parenthesis Expression Nodes
These nodes group sub-expression nodes into a quantified container.

OR Expression Nodes
These nodes also group sub-expression nodes under a quantifier of 0 minimum and 1 maximum occurrence.

Evaluation Engine
The engine is based on the concept of an expression probe. Each probe contains a tree node pointer, a source string offset, a tree node occurrence number, and an expression descent stack. Each probe represents an intention to match the source string at the given offset under the pattern and quantifier of the given tree node.

A probe LIFO queue is maintained of parallel evaluations to be explored by the engine. Probes are evaluated from this queue one at a time until they are killed by either a pattern node match failure, or an occurrence quantifier limit overrun. As the probe proceeds through the expression tree, clone probes are queued to represent choices that are explored after the current probe dies. Each probe will continue through the tree queueing alternative expression probes until the end of the tree is reached.

The probe continues matching at each tree node until the maximum number of occurrences is reached. The probe then back-tracks up its expression stack to continue matching from the previous expression node with the new offset and next occurrence number.

The engine starts by creating and queueing an initial probe for the tree root at source offset zero.

By allowing each probe to match as many source characters as possible before cloned alternative probes are evaluated, and by using the tree node's memo at each alternative choice to prune redundant evaluations, pathological behaviour is precluded for patterns like (a|a?)* applied to the source strings aaaaaaaaaaaaaaaaaaaa or aaaaaaaaaaaaaaaaaaaab.

A memo bit array is maintained for each tree node that records which source offsets have been examined at each node by some probe, and the duplicate descents can be abandoned before they get started.

Author Contact Information
Please address any problems found or questions to the program author, Karl Malbrain: malbrain-at-yahoo-dot-com.
