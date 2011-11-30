// LLVM function pass to canonicalize barriers.
// 
// Copyright (c) 2011 Universidad Rey Juan Carlos
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "CanonicalizeBarriers.h"
#include "Workgroup.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;
using namespace pocl;

#define BARRIER_FUNCTION_NAME "barrier"

static bool is_barrier(Instruction *i);

static Function *barrier = NULL;

namespace {
  static
  RegisterPass<CanonicalizeBarriers> X("barriers",
                                       "Barrier canizalization pass");
}

char CanonicalizeBarriers::ID = 0;

void
CanonicalizeBarriers::getAnalysisUsage(AnalysisUsage &AU) const
{
  AU.addPreserved<DominatorTree>();
  AU.addRequired<LoopInfo>();
  AU.addPreserved<LoopInfo>();
}

bool
CanonicalizeBarriers::doInitialization(Module &M)
{
  barrier = M.getFunction(BARRIER_FUNCTION_NAME);

  return false;
}

bool
CanonicalizeBarriers::runOnFunction(Function &F)
{
  if (!Workgroup::isKernelToProcess(F))
    return false;

  DT = getAnalysisIfAvailable<DominatorTree>();
  LI = &getAnalysis<LoopInfo>();

  bool changed = ProcessFunction(F);

  if (DT)
    DT->verifyAnalysis();
  LI->verifyAnalysis();

  return changed;
}


// Canonicalize barriers: ensure all barriers are in a separate BB
// containint only the barrier and the terminator, with just one
// predecessor and one successor. This allows us to use
// those BBs as markers only, they will not be replicated.
bool
CanonicalizeBarriers::ProcessFunction(Function &F)
{
  bool changed = false;

  InstructionSet Barriers;

  for (Function::iterator i = F.begin(), e = F.end();
       i != e; ++i) {
    BasicBlock *b = i;
    for (BasicBlock::iterator i = b->begin(), e = b->end();
	 i != e; ++i) {
      if (is_barrier(i))
        Barriers.insert(i);
    }
  }

  // Finally add all the split points, now that we are done with the
  // iterators.
  for (InstructionSet::iterator i = Barriers.begin(), e = Barriers.end();
       i != e; ++i) {
    BasicBlock *b = (*i)->getParent();

    // Split post barrier first cause it does not make the barrier
    // to belong to another basic block.
    TerminatorInst  *t = b->getTerminator();
    if ((t->getNumSuccessors() != 1) ||
        (t->getPrevNode() != *i)) {
      BasicBlock *new_b = SplitBlock(b, (*i)->getNextNode(), this);
      new_b->setName(b->getName() + ".postbarrier");
      changed = true;
    }

    BasicBlock *predecessor = b->getSinglePredecessor();
    if (predecessor != NULL) {
      TerminatorInst *pt = predecessor->getTerminator();
      if ((pt->getNumSuccessors() == 1) &&
          (&b->front() == (*i)))
        {
        // Barrier is at the beginning of the BB,
        // which has a single predecessor with just
        // one successor (the barrier itself), thus
        // no need to split before barrier.
        continue;
      }
    }
    BasicBlock *new_b = SplitBlock(b, *i, this);
    new_b->takeName(b);
    b->setName(new_b->getName() + ".prebarrier");
    changed = true;
  }

  return changed;
}


static bool
is_barrier(Instruction *i)
{
  if (CallInst *c = dyn_cast<CallInst>(i)) {
    if (Function *f = c->getCalledFunction()) {
      if (f == barrier)
        return true;
    }
  }

  return false;
}