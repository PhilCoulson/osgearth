/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
* Copyright 2008-2012 Pelican Mapping
* http://osgearth.org
*
* osgEarth is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>
*/
#include <osgEarthUtil/ElevationMorph>
#include <osgEarth/Registry>
#include <osgEarth/Capabilities>
#include <osgEarth/VirtualProgram>
#include <osgEarth/TerrainEngineNode>

#define LC "[ElevationMorph] "

using namespace osgEarth;
using namespace osgEarth::Util;

namespace
{
    // This shader will morph elevation from old heights to new heights as
    // installed in the terrain tile's vertex attributes. attribs[3] holds
    // the new value; attribs2[3] holds the old one. 
    //
    // We use two methods: distance to vertex and time. The morph ratio is
    // a function of the distance from the camera to the vertex (taking into
    // consideration the tile range factor), but when we limit that based on
    // a timer. This prevents fast zooming from skipping the morph altogether.
    //
    // Caveats: You can still fake out the morph by zooming around very quickly.
    // Also, it will only morph properly if you use odd-numbers post spacings
    // in your terrain tile. (See MapOptions::elevation_tile_size).

    const char* vs =
        "attribute vec4 oe_terrain_attr; \n"
        "attribute vec4 oe_terrain_attr2; \n"
        "uniform float oe_min_tile_range_factor; \n"
        "uniform vec4 oe_tile_key; \n"
        "uniform float osg_FrameTime; \n"
        "uniform float oe_tile_birthtime; \n"
        "uniform float oe_morph_delay; \n"
        "uniform float oe_morph_duration; \n"

        "void oe_morph_vertex(inout vec4 VertexMODEL) \n"
        "{ \n"
        "    float far        = oe_min_tile_range_factor; \n"
        "    float near       = far * 0.85; \n"
        "    vec4  VertexVIEW = gl_ModelViewMatrix * VertexMODEL; \n"
        "    float radius     = oe_tile_key.w; \n"
        "    float d          = length(VertexVIEW.xyz/VertexVIEW.w) - radius; \n"
        "    float a          = clamp( d/radius, near, far ); \n"
        "    float r_dist     = ((a-near)/(far-near)); \n"
        "    float r_time     = 1.0 - clamp(osg_FrameTime-(oe_tile_birthtime+oe_morph_delay), 0.0, oe_morph_duration)/oe_morph_duration; \n"
        "    float r          = max(r_dist, r_time); \n"
        "    vec3  upVector   = oe_terrain_attr.xyz; \n"
        "    float elev       = oe_terrain_attr.w; \n"
        "    float elevOld    = oe_terrain_attr2.w; \n"
        "    vec3  offset     = upVector * r * (elevOld - elev); \n"
        "    VertexMODEL      = VertexMODEL + vec4(offset/VertexMODEL.w, 0.0); \n"
        "} \n";
}


ElevationMorph::ElevationMorph() :
TerrainEffect(),
_delay       ( 0.0f ),
_duration    ( 0.25f )
{
    _delayUniform = new osg::Uniform(osg::Uniform::FLOAT, "oe_morph_delay");
    _delayUniform->set( (float)_delay );

    _durationUniform = new osg::Uniform(osg::Uniform::FLOAT, "oe_morph_duration");
    _durationUniform->set( (float)_duration );
}


ElevationMorph::~ElevationMorph()
{
    //nop
}


void
ElevationMorph::setDelay(float delay)
{
    _delay = osg::clampAbove( delay, 0.0f );
    _delayUniform->set( _delay );
}


void
ElevationMorph::setDuration(float duration)
{
    _duration = osg::clampAbove( duration, 0.0f );
    _durationUniform->set( _duration );
}


void
ElevationMorph::onInstall(TerrainEngineNode* engine)
{
    if ( engine )
    {
        osg::StateSet* stateset = engine->getOrCreateStateSet();

        stateset->addUniform( _delayUniform.get() );
        stateset->addUniform( _durationUniform.get() );

        VirtualProgram* vp = VirtualProgram::getOrCreate(stateset);
        vp->setFunction( "oe_morph_vertex", vs, ShaderComp::LOCATION_VERTEX_MODEL );
    }
}


void
ElevationMorph::onUninstall(TerrainEngineNode* engine)
{
    if ( engine )
    {
        osg::StateSet* stateset = engine->getStateSet();
        if ( stateset )
        {
            stateset->removeUniform( _delayUniform.get() );
            stateset->removeUniform( _durationUniform.get() );

            VirtualProgram* vp = VirtualProgram::get(stateset);
            if ( vp )
            {
                vp->removeShader( "oe_morph_vertex" );
            }
        }
    }
}

