// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#include <cstdio>

#include "Core/Reporting.h"
#include "Core/Config.h"
#include "GPU/Directx9/helper/global.h"
#include "GPU/Directx9/PixelShaderGeneratorDX9.h"
#include "GPU/ge_constants.h"
#include "GPU/Common/GPUStateUtils.h"
#include "GPU/GPUState.h"

#define WRITE p+=sprintf

// #define DEBUG_SHADER

namespace DX9 {

// Missing: Z depth range
// Also, logic ops etc, of course, as they are not supported in DX9.
bool GenerateFragmentShaderDX9(const ShaderID &id, char *buffer) {
	char *p = buffer;

	bool lmode = id.Bit(FS_BIT_LMODE);
	bool doTexture = id.Bit(FS_BIT_DO_TEXTURE);
	bool enableFog = id.Bit(FS_BIT_ENABLE_FOG);
	bool enableAlphaTest = id.Bit(FS_BIT_ALPHA_TEST);

	bool alphaTestAgainstZero = id.Bit(FS_BIT_ALPHA_AGAINST_ZERO);
	bool enableColorTest = id.Bit(FS_BIT_COLOR_TEST);
	bool colorTestAgainstZero = id.Bit(FS_BIT_COLOR_AGAINST_ZERO);
	bool enableColorDoubling = id.Bit(FS_BIT_COLOR_DOUBLE);
	bool doTextureProjection = id.Bit(FS_BIT_DO_TEXTURE_PROJ);
	bool doTextureAlpha = id.Bit(FS_BIT_TEXALPHA);
	bool doFlatShading = id.Bit(FS_BIT_FLATSHADE);
	bool isModeClear = id.Bit(FS_BIT_CLEARMODE);

	bool bgraTexture = id.Bit(FS_BIT_BGRA_TEXTURE);

	GEComparison alphaTestFunc = (GEComparison)id.Bits(FS_BIT_ALPHA_TEST_FUNC, 3);
	GEComparison colorTestFunc = (GEComparison)id.Bits(FS_BIT_COLOR_TEST_FUNC, 2);
	bool needShaderTexClamp = id.Bit(FS_BIT_SHADER_TEX_CLAMP);

	ReplaceBlendType replaceBlend = static_cast<ReplaceBlendType>(id.Bits(FS_BIT_REPLACE_BLEND, 3));
	ReplaceAlphaType stencilToAlpha = static_cast<ReplaceAlphaType>(id.Bits(FS_BIT_STENCIL_TO_ALPHA, 2));

	GETexFunc texFunc = (GETexFunc)id.Bits(FS_BIT_TEXFUNC, 3);
	bool textureAtOffset = id.Bit(FS_BIT_TEXTURE_AT_OFFSET);

	GEBlendSrcFactor replaceBlendFuncA = (GEBlendSrcFactor)id.Bits(FS_BIT_BLENDFUNC_A, 4);
	GEBlendDstFactor replaceBlendFuncB = (GEBlendDstFactor)id.Bits(FS_BIT_BLENDFUNC_B, 4);
	GEBlendMode replaceBlendEq = (GEBlendMode)id.Bits(FS_BIT_BLENDEQ, 3);

	StencilValueType replaceAlphaWithStencilType = (StencilValueType)id.Bits(FS_BIT_REPLACE_ALPHA_WITH_STENCIL_TYPE, 4);

	if (doTexture)
		WRITE(p, "sampler tex : register(s0);\n");
	if (!isModeClear && replaceBlend > REPLACE_BLEND_STANDARD) {
		if (replaceBlend == REPLACE_BLEND_COPY_FBO) {
			WRITE(p, "float2 u_fbotexSize : register(c%i);\n", CONST_PS_FBOTEXSIZE);
			WRITE(p, "sampler fbotex : register(s1);\n");
		}
		if (replaceBlendFuncA >= GE_SRCBLEND_FIXA) {
			WRITE(p, "float3 u_blendFixA : register(c%i);\n", CONST_PS_BLENDFIXA);
		}
		if (replaceBlendFuncB >= GE_DSTBLEND_FIXB) {
			WRITE(p, "float3 u_blendFixB : register(c%i);\n", CONST_PS_BLENDFIXB);
		}
	}
	if (gstate_c.needShaderTexClamp && doTexture) {
		WRITE(p, "float4 u_texclamp : register(c%i);\n", CONST_PS_TEXCLAMP);
		if (textureAtOffset) {
			WRITE(p, "float2 u_texclampoff : register(c%i);\n", CONST_PS_TEXCLAMPOFF);
		}
	}

	if (enableAlphaTest || enableColorTest) {
		WRITE(p, "float4 u_alphacolorref : register(c%i);\n", CONST_PS_ALPHACOLORREF);
		WRITE(p, "float4 u_alphacolormask : register(c%i);\n", CONST_PS_ALPHACOLORMASK);
	}
	if (stencilToAlpha && replaceAlphaWithStencilType == STENCIL_VALUE_UNIFORM) {
		WRITE(p, "float u_stencilReplaceValue : register(c%i);\n", CONST_PS_STENCILREPLACE);
	}
	if (doTexture && texFunc == GE_TEXFUNC_BLEND) {
		WRITE(p, "float3 u_texenv : register(c%i);\n", CONST_PS_TEXENV);
	}
	if (enableFog) {
		WRITE(p, "float3 u_fogcolor : register(c%i);\n", CONST_PS_FOGCOLOR);
	}

	if (enableAlphaTest) {
		WRITE(p, "float roundAndScaleTo255f(float x) { return floor(x * 255.0f + 0.5f); }\n");
	}
	if (enableColorTest) {
		WRITE(p, "float3 roundAndScaleTo255v(float3 x) { return floor(x * 255.0f + 0.5f); }\n");
	}

	WRITE(p, "struct PS_IN {\n");
	if (doTexture) {
		if (doTextureProjection)
			WRITE(p, "  float3 v_texcoord: TEXCOORD0;\n");
		else
			WRITE(p, "  float2 v_texcoord: TEXCOORD0;\n");
	}
	WRITE(p, "  float4 v_color0: COLOR0;\n");
	if (lmode) {
		WRITE(p, "  float3 v_color1: COLOR1;\n");
	}
	if (enableFog) {
		WRITE(p, "  float2 v_fogdepth: TEXCOORD1;\n");
	}
	WRITE(p, "};\n");
	WRITE(p, "float4 main( PS_IN In ) : COLOR\n");
	WRITE(p, "{\n");

	if (isModeClear) {
		// Clear mode does not allow any fancy shading.
		WRITE(p, "  float4 v = In.v_color0;\n");
	} else {
		const char *secondary = "";
		// Secondary color for specular on top of texture
		if (lmode) {
			WRITE(p, "  float4 s = float4(In.v_color1, 0);\n");
			secondary = " + s";
		} else {
			secondary = "";
		}

		if (doTexture) {
			const char *texcoord = "In.v_texcoord";
			// TODO: Not sure the right way to do this for projection.
			if (needShaderTexClamp) {
				// We may be clamping inside a larger surface (tex = 64x64, buffer=480x272).
				// We may also be wrapping in such a surface, or either one in a too-small surface.
				// Obviously, clamping to a smaller surface won't work.  But better to clamp to something.
				std::string ucoord = "In.v_texcoord.x";
				std::string vcoord = "In.v_texcoord.y";
				if (doTextureProjection) {
					ucoord = "(In.v_texcoord.x / In.v_texcoord.z)";
					vcoord = "(In.v_texcoord.y / In.v_texcoord.z)";
				}

				if (id.Bit(FS_BIT_CLAMP_S)) {
					ucoord = "clamp(" + ucoord + ", u_texclamp.z, u_texclamp.x - u_texclamp.z)";
				} else {
					ucoord = "fmod(" + ucoord + ", u_texclamp.x)";
				}
				if (id.Bit(FS_BIT_CLAMP_T)) {
					vcoord = "clamp(" + vcoord + ", u_texclamp.w, u_texclamp.y - u_texclamp.w)";
				} else {
					vcoord = "fmod(" + vcoord + ", u_texclamp.y)";
				}
				if (textureAtOffset) {
					ucoord = "(" + ucoord + " + u_texclampoff.x)";
					vcoord = "(" + vcoord + " + u_texclampoff.y)";
				}

				WRITE(p, "  float2 fixedcoord = float2(%s, %s);\n", ucoord.c_str(), vcoord.c_str());
				texcoord = "fixedcoord";
				// We already projected it.
				doTextureProjection = false;
			}

			if (doTextureProjection) {
				WRITE(p, "  float4 t = tex2Dproj(tex, float4(In.v_texcoord.x, In.v_texcoord.y, 0, In.v_texcoord.z))%s;\n", bgraTexture ? ".bgra" : "");
			} else {
				WRITE(p, "  float4 t = tex2D(tex, %s.xy)%s;\n", texcoord, bgraTexture ? ".bgra" : "");
			}
			WRITE(p, "  float4 p = In.v_color0;\n");

			if (doTextureAlpha) { // texfmt == RGBA
				switch (texFunc) {
				case GE_TEXFUNC_MODULATE:
					WRITE(p, "  float4 v = p * t%s;\n", secondary); break;
				case GE_TEXFUNC_DECAL:
					WRITE(p, "  float4 v = float4(lerp(p.rgb, t.rgb, t.a), p.a)%s;\n", secondary); break;
				case GE_TEXFUNC_BLEND:
					WRITE(p, "  float4 v = float4(lerp(p.rgb, u_texenv.rgb, t.rgb), p.a * t.a)%s;\n", secondary); break;
				case GE_TEXFUNC_REPLACE:
					WRITE(p, "  float4 v = t%s;\n", secondary); break;
				case GE_TEXFUNC_ADD:
				case GE_TEXFUNC_UNKNOWN1:
				case GE_TEXFUNC_UNKNOWN2:
				case GE_TEXFUNC_UNKNOWN3:
					WRITE(p, "  float4 v = float4(p.rgb + t.rgb, p.a * t.a)%s;\n", secondary); break;
				default:
					WRITE(p, "  float4 v = p;\n"); break;
				}

			} else {	// texfmt == RGB
				switch (texFunc) {
				case GE_TEXFUNC_MODULATE:
					WRITE(p, "  float4 v = float4(t.rgb * p.rgb, p.a)%s;\n", secondary); break;
				case GE_TEXFUNC_DECAL:
					WRITE(p, "  float4 v = float4(t.rgb, p.a)%s;\n", secondary); break;
				case GE_TEXFUNC_BLEND:
					WRITE(p, "  float4 v = float4(lerp(p.rgb, u_texenv.rgb, t.rgb), p.a)%s;\n", secondary); break;
				case GE_TEXFUNC_REPLACE:
					WRITE(p, "  float4 v = float4(t.rgb, p.a)%s;\n", secondary); break;
				case GE_TEXFUNC_ADD:
				case GE_TEXFUNC_UNKNOWN1:
				case GE_TEXFUNC_UNKNOWN2:
				case GE_TEXFUNC_UNKNOWN3:
					WRITE(p, "  float4 v = float4(p.rgb + t.rgb, p.a)%s;\n", secondary); break;
				default:
					WRITE(p, "  float4 v = p;\n"); break;
				}
			}
		} else {
			// No texture mapping
			WRITE(p, "  float4 v = In.v_color0 %s;\n", secondary);
		}

		if (enableAlphaTest) {
			if (alphaTestAgainstZero) {
				// When testing against 0 (extremely common), we can avoid some math.
				// 0.002 is approximately half of 1.0 / 255.0.
				if (alphaTestFunc == GE_COMP_NOTEQUAL || alphaTestFunc == GE_COMP_GREATER) {
					WRITE(p, "  clip(v.a - 0.002);\n");
				} else if (alphaTestFunc != GE_COMP_NEVER) {
					// Anything else is a test for == 0.  Happens sometimes, actually...
					WRITE(p, "  clip(-v.a + 0.002);\n");
				} else {
					// NEVER has been logged as used by games, although it makes little sense - statically failing.
					// Maybe we could discard the drawcall, but it's pretty rare.  Let's just statically discard here.
					WRITE(p, "  clip(-1);\n");
				}
			} else {
				const char *alphaTestFuncs[] = { "#", "#", " != ", " == ", " >= ", " > ", " <= ", " < " };	// never/always don't make sense
				if (alphaTestFuncs[alphaTestFunc][0] != '#') {
					// TODO: Rewrite this to use clip() appropriately (like, clip(v.a - u_alphacolorref.a))
					WRITE(p, "  if (roundAndScaleTo255f(v.a) %s u_alphacolorref.a) clip(-1);\n", alphaTestFuncs[alphaTestFunc]);
				} else {
					// This means NEVER.  See above.
					WRITE(p, "  clip(-1);\n");
				}
			}
		}
		if (enableColorTest) {
			if (colorTestAgainstZero) {
				// When testing against 0 (common), we can avoid some math.
				// 0.002 is approximately half of 1.0 / 255.0.
				if (colorTestFunc == GE_COMP_NOTEQUAL) {
					WRITE(p, "  if (v.r < 0.002 && v.g < 0.002 && v.b < 0.002) clip(-1);\n");
				} else if (colorTestFunc != GE_COMP_NEVER) {
					// Anything else is a test for == 0.
					WRITE(p, "  if (v.r > 0.002 || v.g > 0.002 || v.b > 0.002) clip(-1);\n");
				} else {
					// NEVER has been logged as used by games, although it makes little sense - statically failing.
					// Maybe we could discard the drawcall, but it's pretty rare.  Let's just statically discard here.
					WRITE(p, "  clip(-1);\n");
				}
			} else {
				const char *colorTestFuncs[] = { "#", "#", " != ", " == " };	// never/always don't make sense
				if (colorTestFuncs[colorTestFunc][0] != '#') {
					const char * test = colorTestFuncs[colorTestFunc];
					WRITE(p, "  float3 colortest = roundAndScaleTo255v(v.rgb);\n");
					WRITE(p, "  if ((colortest.r %s u_alphacolorref.r) && (colortest.g %s u_alphacolorref.g) && (colortest.b %s u_alphacolorref.b )) clip(-1);\n", test, test, test);
				} else {
					WRITE(p, "  clip(-1);\n");
				}
			}
		}

		// Color doubling happens after the color test.
		if (enableColorDoubling && replaceBlend == REPLACE_BLEND_2X_SRC) {
			WRITE(p, "  v.rgb = v.rgb * 4.0;\n");
		} else if (enableColorDoubling || replaceBlend == REPLACE_BLEND_2X_SRC) {
			WRITE(p, "  v.rgb = v.rgb * 2.0;\n");
		}

		if (enableFog) {
			WRITE(p, "  float fogCoef = clamp(In.v_fogdepth.x, 0.0, 1.0);\n");
			WRITE(p, "  v = lerp(float4(u_fogcolor, v.a), v, fogCoef);\n");
		}

		if (replaceBlend == REPLACE_BLEND_PRE_SRC || replaceBlend == REPLACE_BLEND_PRE_SRC_2X_ALPHA) {
			const char *srcFactor = "ERROR";
			switch (replaceBlendFuncA) {
			case GE_SRCBLEND_DSTCOLOR:          srcFactor = "ERROR"; break;
			case GE_SRCBLEND_INVDSTCOLOR:       srcFactor = "ERROR"; break;
			case GE_SRCBLEND_SRCALPHA:          srcFactor = "float3(v.a, v.a, v.a)"; break;
			case GE_SRCBLEND_INVSRCALPHA:       srcFactor = "float3(1.0 - v.a, 1.0 - v.a, 1.0 - v.a)"; break;
			case GE_SRCBLEND_DSTALPHA:          srcFactor = "ERROR"; break;
			case GE_SRCBLEND_INVDSTALPHA:       srcFactor = "ERROR"; break;
			case GE_SRCBLEND_DOUBLESRCALPHA:    srcFactor = "float3(v.a * 2.0, v.a * 2.0, v.a * 2.0)"; break;
			case GE_SRCBLEND_DOUBLEINVSRCALPHA: srcFactor = "float3(1.0 - v.a * 2.0, 1.0 - v.a * 2.0, 1.0 - v.a * 2.0)"; break;
			// PRE_SRC for REPLACE_BLEND_PRE_SRC_2X_ALPHA means "double the src."
			// It's close to the same, but clamping can still be an issue.
			case GE_SRCBLEND_DOUBLEDSTALPHA:    srcFactor = "float3(2.0, 2.0, 2.0)"; break;
			case GE_SRCBLEND_DOUBLEINVDSTALPHA: srcFactor = "ERROR"; break;
			case GE_SRCBLEND_FIXA:              srcFactor = "u_blendFixA"; break;
			default:                            srcFactor = "u_blendFixA"; break;
			}

			WRITE(p, "  v.rgb = v.rgb * %s;\n", srcFactor);
		}

		// Can do REPLACE_BLEND_COPY_FBO in ps_2_0, but need to apply viewport in the vertex shader
		// so that we can have the output position here to sample the texture at.

		if (replaceBlend == REPLACE_BLEND_2X_ALPHA || replaceBlend == REPLACE_BLEND_PRE_SRC_2X_ALPHA) {
			WRITE(p, "  v.a = v.a * 2.0;\n");
		}
	}

	std::string replacedAlpha = "0.0";
	char replacedAlphaTemp[64] = "";
	if (stencilToAlpha != REPLACE_ALPHA_NO) {
		switch (replaceAlphaWithStencilType) {
		case STENCIL_VALUE_UNIFORM:
			replacedAlpha = "u_stencilReplaceValue";
			break;

		case STENCIL_VALUE_ZERO:
			replacedAlpha = "0.0";
			break;

		case STENCIL_VALUE_ONE:
		case STENCIL_VALUE_INVERT:
			// In invert, we subtract by one, but we want to output one here.
			replacedAlpha = "1.0";
			break;

		case STENCIL_VALUE_INCR_4:
		case STENCIL_VALUE_DECR_4:
			// We're adding/subtracting, just by the smallest value in 4-bit.
			snprintf(replacedAlphaTemp, sizeof(replacedAlphaTemp), "%f", 1.0 / 15.0);
			replacedAlpha = replacedAlphaTemp;
			break;

		case STENCIL_VALUE_INCR_8:
		case STENCIL_VALUE_DECR_8:
			// We're adding/subtracting, just by the smallest value in 8-bit.
			snprintf(replacedAlphaTemp, sizeof(replacedAlphaTemp), "%f", 1.0 / 255.0);
			replacedAlpha = replacedAlphaTemp;
			break;

		case STENCIL_VALUE_KEEP:
			// Do nothing. We'll mask out the alpha using color mask.
			break;
		}
	}

	switch (stencilToAlpha) {
	case REPLACE_ALPHA_DUALSOURCE:
		WRITE(p, "  v.a = %s;\n", replacedAlpha.c_str());
		// TODO: Output the second color as well using original v.a.
		break;

	case REPLACE_ALPHA_YES:
		WRITE(p, "  v.a = %s;\n", replacedAlpha.c_str());
		break;

	case REPLACE_ALPHA_NO:
		// Do nothing, v is already fine.
		break;
	}

	LogicOpReplaceType replaceLogicOpType = (LogicOpReplaceType)id.Bits(FS_BIT_REPLACE_LOGIC_OP_TYPE, 2);
	switch (replaceLogicOpType) {
	case LOGICOPTYPE_ONE:
		WRITE(p, "  v.rgb = float3(1.0, 1.0, 1.0);\n");
		break;
	case LOGICOPTYPE_INVERT:
		WRITE(p, "  v.rgb = float3(1.0, 1.0, 1.0) - v.rgb;\n");
		break;
	case LOGICOPTYPE_NORMAL:
		break;
	}

	WRITE(p, "  return v;\n");
	WRITE(p, "}\n");
	return true;
}

};
