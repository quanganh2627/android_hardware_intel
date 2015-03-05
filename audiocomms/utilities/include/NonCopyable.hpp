/**
 * @file
 * class NonCopyable to limit copy on objects.
 *
 * @section License
 *
 * Copyright 2013 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

namespace audio_comms
{

namespace utilities
{

/**
 * Class NonCopyable gives an easy way to prevent an object to be copyable.
 * Any class that want to be non-copyable would just need to inherit from
 * this class.
 */
class NonCopyable
{
protected:
    /**
     * Constructors and destructors are protected because this call is
     * meaningless by itself. */
    NonCopyable() {}
    ~NonCopyable() {}

private:
    /* Copy facilities are put private to disable copy. */
    NonCopyable(const NonCopyable &object);
    NonCopyable &operator=(const NonCopyable &object);
};

} // namespace utilities

} // namespace audio_comms
