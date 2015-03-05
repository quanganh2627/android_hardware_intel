/*
 * Copyright (C) 2013 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _ATOM_FIFO_H_
#define _ATOM_FIFO_H_

template <class X> class AtomFifo {
    X *buffer;
    unsigned int depth;
    unsigned int count;
    unsigned int eqIdx; // enqueue index
    unsigned int dqIdx; // dequeue index

public:
    AtomFifo(unsigned int depth);
    virtual ~AtomFifo();
    unsigned int getDepth() { return depth; };
    int enqueue(X& val);
    X* peek(unsigned int index);
    int dequeue(X *val = NULL);
    void reset();
    unsigned int getCount();
};

template <class X> AtomFifo<X>::AtomFifo(unsigned int depth)
{
    buffer = NULL;
    if (depth == 0)
        return;

    this->depth = depth;
    buffer = new X[depth];
    reset();
}

template <class X> AtomFifo<X>::~AtomFifo()
{
    if (buffer) {
        delete[] buffer;
        buffer = NULL;
    }
}

template <class X> int AtomFifo<X>::enqueue(X& val)
{
    if (count == depth)
        return -1;  // full !
    buffer[eqIdx++] = val;
    count++;
    if (eqIdx >= depth)
        eqIdx = 0;
    return 0;
}

template <class X> X* AtomFifo<X>::peek(unsigned int index)
{
    if (index >= count)
        return NULL;
    return &(buffer[(eqIdx + depth - 1 - index) % depth]);
}

template <class X> int AtomFifo<X>::dequeue(X *val)
{
    if (count == 0)
        return -1;  // empty !
    if (val)
        *val = buffer[dqIdx];
    dqIdx++;
    count--;
    if (dqIdx >= depth)
        dqIdx = 0;
    return 0;
}

template <class X> void AtomFifo<X>::reset()
{
    count = 0;
    eqIdx = 0;
    dqIdx = 0;
}

template <class X> unsigned int AtomFifo<X>::getCount()
{
    return count;
}

#endif /* _ATOM_FIFO_H_ */
