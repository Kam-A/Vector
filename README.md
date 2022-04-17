# Vector implementation


  - Implemented via RawMemory class and placement new operator
  - Strong exception safety
  - Optimal number of calls constructor, destructor, copy-/move- constructor(see Benchmark() test)
  - Emplace with variadic templates