# typed: __STDLIB_INTERNAL

class Struct < Object
  include Enumerable

  extend T::Generic
  Elem = type_member(:out, fixed: T.untyped)
end
