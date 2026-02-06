
problem: 
grpc-src/src/core/lib/promise/detail/basic_seq.h:103:38: error: a template argument list is expected after a name prefixed by the template keyword [-Wmissing-template-arg-list-after-template-kw] 103 | Traits::template CallSeqFactory(f_, *cur_, std::move(arg)));

fix recipe:

gRPC-Core/src/core/lib/promise/detail/basic_seq.h
Step 2: Edit the Code
Go to the line mentioned in the error, which is around line 102. You will see the following code:
// Before the fix
return Traits::template CallSeqFactory(f_, *cur_, std::move(arg));
All you need to do is add <> right after CallSeqFactory.
// After the fix
return Traits::template CallSeqFactory<>(f_, *cur_, std::move(arg));

file @ build/_deps/grpc-src/src/core/lib/promise/detail/basic_seq.h is patched alreadsy
but you should modifiy the dep cmake scripts to path it again 

a copy is saved in instructions/basic_seq.h
