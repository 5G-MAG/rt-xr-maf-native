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

// The picture importers of the graphics APIs

#pragma once

#include <memory>

#include "TextureRegistry.hpp"

#if defined(__APPLE__) && defined(__OBJC__)
#import <Metal/Metal.h>
// Imports CVPixelBuffers as Metal textures, one per plane, without a copy
std::unique_ptr<PictureImporter> CreateMetalImporter(id<MTLDevice> device);
#endif
