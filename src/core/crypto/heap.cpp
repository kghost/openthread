/*
 *  Copyright (c) 2017, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file implements heap.
 *
 */

#include "heap.hpp"

#include <assert.h>
#include <string.h>

#include "common/code_utils.hpp"

namespace ot {
namespace Crypto {

Heap::Heap(void)
{
    Block &super = BlockAt(kSuperBlockOffset);
    super.SetSize(kSuperBlockSize);

    Block &first = BlockRight(super);
    first.SetSize(kFirstBlockSize);

    Block &guard = BlockRight(first);
    guard.SetSize(Block::kGuardBlockSize);

    super.SetNext(BlockOffset(first));
    first.SetNext(BlockOffset(guard));
}

void *Heap::CAlloc(size_t aCount, size_t aSize)
{
    void *ret = NULL;
    Block *prev = NULL;
    Block *curr = NULL;
    uint16_t size = static_cast<uint16_t>(aCount * aSize);

    VerifyOrExit(size);

    size += kAlignSize - 1 - kBlockRemainderSize;
    size &= ~(kAlignSize - 1);
    size += kBlockRemainderSize;

    prev = &BlockSuper();
    curr = &BlockNext(*prev);

    while (curr->GetSize() < size)
    {
        prev = curr;
        curr = &BlockNext(*curr);
    }

    VerifyOrExit(curr->IsFree());

    prev->SetNext(curr->GetNext());

    if (curr->GetSize() > size + sizeof(Block))
    {
        const uint16_t newBlockSize = curr->GetSize() - size - sizeof(Block);
        curr->SetSize(size);

        Block &newBlock = BlockRight(*curr);
        newBlock.SetSize(newBlockSize);
        newBlock.SetNext(0);

        if (prev->GetSize() < newBlockSize)
        {
            BlockInsert(*prev, newBlock);
        }
        else
        {
            BlockInsert(BlockSuper(), newBlock);
        }
    }

    curr->SetNext(0);

    memset(curr->GetPointer(), 0, size);
    ret = curr->GetPointer();

exit:
    return ret;
}

void Heap::BlockInsert(Block &aPrev, Block &aBlock)
{
    Block *prev = &aPrev;

    for (Block *block = &BlockNext(*prev);
         block->GetSize() < aBlock.GetSize();
         block = &BlockNext(*block))
    {
        prev = block;
    }

    aBlock.SetNext(prev->GetNext());
    prev->SetNext(BlockOffset(aBlock));
}

Block &Heap::BlockPrev(const Block &aBlock)
{
    Block *prev = &BlockSuper();

    while (prev->GetNext() != BlockOffset(aBlock))
    {
        prev = &BlockNext(*prev);
    }

    return *prev;
}

void Heap::Free(void *aPointer)
{
    if (aPointer == NULL)
    {
        return;
    }

    Block &block = BlockOf(aPointer);
    Block &right = BlockRight(block);

    if (IsLeftFree(block))
    {
        Block *prev = &BlockSuper();
        Block *left = &BlockNext(*prev);

        for (const uint16_t offset = block.GetLeftNext(); left->GetNext() != offset; left = &BlockNext(*left))
        {
            prev = left;
        }

        // Remove left from free list.
        prev->SetNext(left->GetNext());
        left->SetNext(0);

        if (right.IsFree())
        {
            if (right.GetSize() > left->GetSize())
            {
                for (const uint16_t offset = BlockOffset(right); prev->GetNext() != offset; prev = &BlockNext(*prev));
            }
            else
            {
                prev = &BlockPrev(right);
            }

            // Remove right from free list.
            prev->SetNext(right.GetNext());
            right.SetNext(0);

            // Add size of right.
            left->SetSize(left->GetSize() + right.GetSize() + sizeof(Block));
        }

        // Add size of current block.
        left->SetSize(left->GetSize() + block.GetSize() + sizeof(Block));

        BlockInsert(*prev, *left);
    }
    else
    {
        if (right.IsFree())
        {
            Block &prev = BlockPrev(right);
            prev.SetNext(right.GetNext());
            block.SetSize(block.GetSize()  + right.GetSize() + sizeof(Block));
            BlockInsert(prev, block);
        }
        else
        {
            BlockInsert(BlockSuper(), block);
        }
    }
}

}  // namespace Crypto
}  // namespace ot
