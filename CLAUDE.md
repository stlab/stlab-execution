# Concurrency

- Always give mutex locks the narrowest possible scope that preserves the complete
  synchronization invariant. Keep unrelated work, callbacks, capture destruction,
  and blocking operations outside the critical section whenever safe.
- At each mutex declaration, comment which state or synchronization invariant it
  protects, including any condition-variable or object-lifetime coordination.
- If a mutex protects only a single member, consider an atomic instead. Do not
  replace a mutex when compound operations, condition-variable waits, or lifetime
  guarantees still require it.
