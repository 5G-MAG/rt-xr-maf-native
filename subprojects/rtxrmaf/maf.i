/*
 * Copyright (c) 2023 MotionSpell
 * Licensed under the License terms and conditions for use, reproduction,
 * and distribution of 5GMAG software (the “License”).
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at https://www.5g-mag.com/license .
 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an “AS IS” BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and limitations under the License.
 */


#define SWIG_BINDINGS
%include <typemaps.i>

%module maf

%{
#include "maf.hpp"
#include "factory.hpp"
%}

%include "std_string.i"
%include "std_vector.i"
%include "stdint.i"

namespace std {
  %template(BufferInfoArray) vector<MAF::BufferInfo>;
  %template(BufferInfoHeaderArray) vector<MAF::BufferInfoHeader>;
  %template(MediaLocationArray) vector<MAF::AlternativeLocation>;
  %template(FrameArray) vector<MAF::Frame>;
  %template(TrackArray) vector<MAF::Track>;
};


%apply void * VOID_INT_PTR { uint8_t * };
%apply void * VOID_INT_PTR { void * };
%apply int &OUTPUT { int &bytelength };
# %apply int &INOUT std::vector<BufferInfoHeader>* headers

/* 
Tell SWIG that MediaPipelineFactory::createMediaPipeline creates a new object, 
otherwise ownership (swigCMemOwn) is set to false and auto generated IMediaPipeline::Dispose() call fails to properly destroy the pipeline
*/
%newobject MediaPipelineFactory::createMediaPipeline;

/* Parse & generate wrappers */
%include "maf.hpp";
%include "factory.hpp";
