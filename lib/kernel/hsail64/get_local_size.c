/* OpenCL built-in library: get_local_size() for HSAIL

   Copyright (c) 2015 Pekka Jääskeläinen of TUT

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

extern size_t _local_size_x;
extern size_t _local_size_y;
extern size_t _local_size_z;

size_t _CL_OVERLOADABLE
get_local_size(unsigned int dimindx)
{
  switch(dimindx)
    {
    case 0: return  __builtin_hsail_currentworkgroupsize(0);
    case 1: return  __builtin_hsail_currentworkgroupsize(1);
    case 2: return  __builtin_hsail_currentworkgroupsize(2);
    default: return 0;
    }
}

