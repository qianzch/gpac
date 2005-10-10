/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / SVG Loader module
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *   
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */


#include <gpac/internal/terminal_dev.h>
#include "svg_parser.h"

#ifndef GPAC_DISABLE_SVG

static GF_Err LSR_ProcessDocument(GF_SceneDecoder *plug, unsigned char *inBuffer, u32 inBufferLength, 
								u16 ES_ID, u32 stream_time, u32 mmlevel)
{
	GF_Err e;
	SVGParser *parser = plug->privateStack;

	parser->stream_time = stream_time;
	e = SVGParser_ParseLASeR(parser);
	if (!e && parser->needs_attachement) {
		parser->needs_attachement = 0;
		gf_sg_set_scene_size_info(parser->graph, parser->svg_w, parser->svg_h, 1);
		gf_is_attach_to_renderer(parser->inline_scene);
	}
	return e;
}

/* Only in case of reading from file (cached or not) of an XML file (i.e. not AU framed)
   The buffer is empty but the filename has been given in a previous step: SVG_AttachStream */
static GF_Err SVG_ProcessDocument(GF_SceneDecoder *plug, unsigned char *inBuffer, u32 inBufferLength, 
								u16 ES_ID, u32 stream_time, u32 mmlevel)
{
	GF_Err e = GF_OK;
	SVGParser *parser = plug->privateStack;

	if (parser->status == 0) {
		parser->status = 1;
		if (parser->oti == SVGLOADER_OTI_FULL_SVG) 
			e = SVGParser_ParseFullDoc(parser);
		if (parser->oti == SVGLOADER_OTI_PROGRESSIVE_SVG) 
			e = SVGParser_ParseProgressiveDoc(parser);
		
		if (!e) {
			gf_sg_set_scene_size_info(parser->graph, parser->svg_w, parser->svg_h, 1);
			/*attach graph to renderer*/
			gf_is_attach_to_renderer(parser->inline_scene);
		} else {
			parser->status = 0;
			return e;
		}
	} else {
		if (parser->oti == SVGLOADER_OTI_PROGRESSIVE_SVG) 
			e = SVGParser_ParseProgressiveDoc(parser);
	}
	return GF_EOS;
}

/* Only in case of streaming or reading from MP4 file or framed container
   The buffer contains the actual piece of SVG to read */
static GF_Err SVG_ProcessAU(GF_SceneDecoder *plug, unsigned char *inBuffer, u32 inBufferLength, 
								u16 ES_ID, u32 stream_time, u32 mmlevel)
{

}

static GF_Err SVG_ProcessData(GF_SceneDecoder *plug, unsigned char *inBuffer, u32 inBufferLength, 
								u16 ES_ID, u32 stream_time, u32 mmlevel)
{
	SVGParser *parser = plug->privateStack;
	if (parser->oti == SVGLOADER_OTI_FULL_SVG) 
		return SVG_ProcessDocument(plug, inBuffer, inBufferLength, ES_ID, stream_time, mmlevel);
	if (parser->oti == SVGLOADER_OTI_PROGRESSIVE_SVG) 
		return SVG_ProcessDocument(plug, inBuffer, inBufferLength, ES_ID, stream_time, mmlevel);
	if (parser->oti == SVGLOADER_OTI_STREAMING_SVG) 
		return SVG_ProcessAU(plug, inBuffer, inBufferLength, ES_ID, stream_time, mmlevel);
	if (parser->oti==SVGLOADER_OTI_FULL_LASERML) 
		return LSR_ProcessDocument(plug, inBuffer, inBufferLength, ES_ID, stream_time, mmlevel);
	return GF_BAD_PARAM;
}

static GF_Err SVG_AttachScene(GF_SceneDecoder *plug, GF_InlineScene *scene, Bool is_scene_decoder)
{
	SVGParser *parser = plug->privateStack;
	parser->inline_scene = scene;
	parser->graph = scene->graph;
	parser->temp_dir = (char *) gf_modules_get_option((GF_BaseInterface *)plug, "General", "CacheDirectory");
	return GF_OK;
}

static GF_Err SVG_ReleaseScene(GF_SceneDecoder *plug)
{
	return GF_OK;
}


static GF_Err SVG_AttachStream(GF_BaseDecoder *plug, 
									 u16 ES_ID, 
									 unsigned char *decSpecInfo, 
									 u32 decSpecInfoSize, 
									 u16 DependsOnES_ID,
									 u32 objectTypeIndication, 
									 Bool Upstream)
{
	SVGParser *parser = plug->privateStack;
	if (Upstream) return GF_NOT_SUPPORTED;

	/* decSpecInfo is not null only when reading from an SVG file (local or distant, cached or not) */
	if (!decSpecInfo && objectTypeIndication != SVGLOADER_OTI_STREAMING_SVG) return GF_NON_COMPLIANT_BITSTREAM;
	else parser->fileName = strdup(decSpecInfo);

	parser->oti = objectTypeIndication;

	return GF_OK;
}

static GF_Err SVG_DetachStream(GF_BaseDecoder *plug, u16 ES_ID)
{
	return GF_OK;
}

const char *SVG_GetName(struct _basedecoder *plug)
{
	SVGParser *parser = plug->privateStack;
	if (parser->oti==SVGLOADER_OTI_FULL_SVG) return "GPAC SVG Parser";
	if (parser->oti==SVGLOADER_OTI_PROGRESSIVE_SVG) return "GPAC SVG Progressive Parser";
	if (parser->oti==SVGLOADER_OTI_STREAMING_SVG) return "GPAC Streaming SVG Parser";
	if (parser->oti==SVGLOADER_OTI_FULL_LASERML) return "GPAC LASeRML Parser";
	return "INTERNAL ERROR";
}

Bool SVG_CanHandleStream(GF_BaseDecoder *ifce, u32 StreamType, u32 ObjectType, unsigned char *decSpecInfo, u32 decSpecInfoSize, u32 PL)
{
	if (StreamType!=GF_STREAM_PRIVATE_SCENE) return 0;
	if (ObjectType==SVGLOADER_OTI_FULL_SVG) return 1;
	if (ObjectType==SVGLOADER_OTI_PROGRESSIVE_SVG) return 1;
	if (ObjectType==SVGLOADER_OTI_STREAMING_SVG) return 1;
	if (ObjectType==SVGLOADER_OTI_FULL_LASERML) return 1;
	return 0;
}

static GF_Err SVG_GetCapabilities(GF_BaseDecoder *plug, GF_CodecCapability *cap)
{
	cap->cap.valueInt = 0;
	return GF_NOT_SUPPORTED;
}

static GF_Err SVG_SetCapabilities(GF_BaseDecoder *plug, const GF_CodecCapability capability)
{
	return GF_OK;
}

/*interface create*/
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	SVGParser *parser;
	GF_SceneDecoder *sdec;
	if (InterfaceType != GF_SCENE_DECODER_INTERFACE) return NULL;
	
	GF_SAFEALLOC(sdec, sizeof(GF_SceneDecoder))
	GF_REGISTER_MODULE_INTERFACE(sdec, GF_SCENE_DECODER_INTERFACE, "GPAC SVG Parser", "gpac distribution");

	parser = NewSVGParser();

	sdec->privateStack = parser;
#ifdef USE_GPAC_CACHE_MECHANISM
	parser->sdec = sdec;
#endif
	sdec->AttachStream = SVG_AttachStream;
	sdec->CanHandleStream = SVG_CanHandleStream;
	sdec->DetachStream = SVG_DetachStream;
	sdec->AttachScene = SVG_AttachScene;
	sdec->ReleaseScene = SVG_ReleaseScene;
	sdec->ProcessData = SVG_ProcessData;
	sdec->GetName = SVG_GetName;
	sdec->SetCapabilities = SVG_SetCapabilities;
	sdec->GetCapabilities = SVG_GetCapabilities;
	return (GF_BaseInterface *)sdec;
}


/*interface destroy*/
void ShutdownInterface(GF_BaseInterface *ifce)
{
	GF_SceneDecoder *sdec = (GF_SceneDecoder *)ifce;
	SVGParser *parser = (SVGParser *) sdec->privateStack;
	if (sdec->InterfaceType != GF_SCENE_DECODER_INTERFACE) return;

	SVGParser_Terminate(parser);
	free(sdec);
}

/*interface query*/
Bool QueryInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_SCENE_DECODER_INTERFACE) return 1;
	return 0;
}
#else


/*interface create*/
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	return NULL;
}


/*interface destroy*/
void ShutdownInterface(GF_BaseInterface *ifce)
{
}

/*interface query*/
Bool QueryInterface(u32 InterfaceType)
{
	return 0;
}
#endif
