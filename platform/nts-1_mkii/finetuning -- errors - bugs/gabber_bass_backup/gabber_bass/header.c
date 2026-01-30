/*
    BSD 3-Clause License

    Copyright (c) 2023, KORG INC.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this
      list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    * Neither the name of the copyright holder nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

//*/

/*
 *  File: header.c
 *
 *  NTS-1 mkII oscillator unit header definition
 *  GABBER BASS - Turn Up The Bass inspired hardcore oscillator
 *
 */

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x1U,
    .version = 0x00010000U,
    .name = "GABBER",
    .num_params = 10,
    .params = {
        {0, 1023, 512, 512, k_unit_param_type_percent, 0, 0, 0, {"DIST"}},
        {0, 7, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"MODE"}},
        {0, 1023, 512, 768, k_unit_param_type_percent, 0, 0, 0, {"PENV"}},
        {0, 1023, 512, 512, k_unit_param_type_percent, 0, 0, 0, {"SUB"}},
        {0, 1023, 512, 512, k_unit_param_type_percent, 0, 0, 0, {"DETUNE"}},
        {0, 1023, 512, 768, k_unit_param_type_percent, 0, 0, 0, {"CUTOFF"}},
        {0, 1023, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"CRUSH"}},
        {0, 1023, 512, 307, k_unit_param_type_percent, 0, 0, 0, {"BOUNCE"}},
        {0, 1023, 512, 512, k_unit_param_type_percent, 0, 0, 0, {"WIDE"}},
        {0, 1023, 512, 205, k_unit_param_type_percent, 0, 0, 0, {"GLIDE"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}},
};

