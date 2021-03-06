/*
-----------------------------------------------------------------------------
This source file is part of OGRE
    (Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2008 Renato Araujo Oliveira Filho <renatox@gmail.com>
Copyright (c) 2000-2014 Torus Knot Software Ltd

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
-----------------------------------------------------------------------------
*/

#include "OgreException.h"
#include "OgreLogManager.h"
#include "OgreStringConverter.h"
#include "OgreRoot.h"

#include "OgreRenderSystem.h"

#include "OgreEGLSupport.h"
#include "OgreEGLWindow.h"
#include "OgreEGLRenderTexture.h"


namespace Ogre {


    EGLSupport::EGLSupport(int profile)
        : GLNativeSupport(profile), mGLDisplay(0),
          mNativeDisplay(0),
      mRandr(false)
    {
    }

    void EGLSupport::addConfig(void)
    {
        ConfigOption optFullScreen;
        ConfigOption optVideoMode;
        ConfigOption optDisplayFrequency;
        ConfigOption optFSAA;
        ConfigOption optVSync;

        optFullScreen.name = "Full Screen";
        optFullScreen.immutable = false;

        optVideoMode.name = "Video Mode";
        optVideoMode.immutable = false;

        optDisplayFrequency.name = "Display Frequency";
        optDisplayFrequency.immutable = false;

        optVSync.name = "VSync";
        optVSync.possibleValues.push_back("No");
        optVSync.possibleValues.push_back("Yes");
        optVSync.currentValue = optVSync.possibleValues[1];
        optVSync.immutable = false;

        optFSAA.name = "FSAA";
        optFSAA.immutable = false;

        optFullScreen.possibleValues.push_back("No");
        optFullScreen.possibleValues.push_back("Yes");

        optFullScreen.currentValue = optFullScreen.possibleValues[0];

        VideoModes::const_iterator value = mVideoModes.begin();
        VideoModes::const_iterator end = mVideoModes.end();

        for (; value != end; value++)
        {
            String mode = StringConverter::toString(value->first.first,4) + " x " + StringConverter::toString(value->first.second,4);
            optVideoMode.possibleValues.push_back(mode);
        }
        removeDuplicates(optVideoMode.possibleValues);

        optVideoMode.currentValue = StringConverter::toString(mCurrentMode.first.first,4) + " x " + StringConverter::toString(mCurrentMode.first.second,4);

        refreshConfig();
        if (!mSampleLevels.empty())
        {
            StringVector::const_iterator sampleValue = mSampleLevels.begin();
            StringVector::const_iterator sampleEnd = mSampleLevels.end();

            for (; sampleValue != sampleEnd; sampleValue++)
            {
                optFSAA.possibleValues.push_back(*sampleValue);
            }

            optFSAA.currentValue = optFSAA.possibleValues[0];
        }

        mOptions[optFullScreen.name] = optFullScreen;
        mOptions[optVideoMode.name] = optVideoMode;
        mOptions[optDisplayFrequency.name] = optDisplayFrequency;
        mOptions[optFSAA.name] = optFSAA;
        mOptions[optVSync.name] = optVSync;

        refreshConfig();
    }

    void EGLSupport::refreshConfig(void) 
    {
        ConfigOptionMap::iterator optVideoMode = mOptions.find("Video Mode");
        ConfigOptionMap::iterator optDisplayFrequency = mOptions.find("Display Frequency");

        if (optVideoMode != mOptions.end() && optDisplayFrequency != mOptions.end())
        {
            optDisplayFrequency->second.possibleValues.clear();

            VideoModes::const_iterator value = mVideoModes.begin();
            VideoModes::const_iterator end = mVideoModes.end();

            for (; value != end; value++)
            {
                String mode = StringConverter::toString(value->first.first,4) + " x " + StringConverter::toString(value->first.second,4);

                if (mode == optVideoMode->second.currentValue)
                {
                    String frequency = StringConverter::toString(value->second) + " MHz";

                    optDisplayFrequency->second.possibleValues.push_back(frequency);
                }
            }

            if (!optDisplayFrequency->second.possibleValues.empty())
            {
                optDisplayFrequency->second.currentValue = optDisplayFrequency->second.possibleValues[0];
            }
            else
            {
                optVideoMode->second.currentValue = StringConverter::toString(mVideoModes[0].first.first,4) + " x " + StringConverter::toString(mVideoModes[0].first.second,4);
                optDisplayFrequency->second.currentValue = StringConverter::toString(mVideoModes[0].second) + " MHz";
            }
        }
    }

    void EGLSupport::setConfigOption(const String &name, const String &value)
    {
        GLNativeSupport::setConfigOption(name, value);
        if (name == "Video Mode")
        {
            refreshConfig();
        }
    }

    EGLDisplay EGLSupport::getGLDisplay(void)
    {
        mGLDisplay = eglGetDisplay(mNativeDisplay);
        EGL_CHECK_ERROR

        if(mGLDisplay == EGL_NO_DISPLAY)
        {
            OGRE_EXCEPT(Exception::ERR_RENDERINGAPI_ERROR,
                        "Couldn`t open EGLDisplay " + getDisplayName(),
                        "EGLSupport::getGLDisplay");
        }

        if (eglInitialize(mGLDisplay, &mEGLMajor, &mEGLMinor) == EGL_FALSE)
        {
            OGRE_EXCEPT(Exception::ERR_RENDERINGAPI_ERROR,
                        "Couldn`t initialize EGLDisplay ",
                        "EGLSupport::getGLDisplay");
        }
        EGL_CHECK_ERROR

        return mGLDisplay;
    }


    String EGLSupport::getDisplayName(void)
    {
        return "todo";
    }

    EGLConfig* EGLSupport::chooseGLConfig(const EGLint *attribList, EGLint *nElements)
    {
        EGLConfig *configs;

        if (eglChooseConfig(mGLDisplay, attribList, NULL, 0, nElements) == EGL_FALSE)
        {
            OGRE_EXCEPT(Exception::ERR_RENDERINGAPI_ERROR,
                        "Failed to choose config",
                        __FUNCTION__);

            *nElements = 0;
            return 0;
        }
        EGL_CHECK_ERROR
        configs = (EGLConfig*) malloc(*nElements * sizeof(EGLConfig));
        if (eglChooseConfig(mGLDisplay, attribList, configs, *nElements, nElements) == EGL_FALSE)
        {
            OGRE_EXCEPT(Exception::ERR_RENDERINGAPI_ERROR,
                        "Failed to choose config",
                        __FUNCTION__);

            *nElements = 0;
            free(configs);
            return 0;
        }
        EGL_CHECK_ERROR
        return configs;
    }

    EGLConfig* EGLSupport::getConfigs(EGLint *nElements)
    {
        EGLConfig *configs;

        if (eglGetConfigs(mGLDisplay, NULL, 0, nElements) == EGL_FALSE)
        {
            OGRE_EXCEPT(Exception::ERR_RENDERINGAPI_ERROR,
                        "Failed to choose config",
                        __FUNCTION__);

            *nElements = 0;
            return 0;
        }
        EGL_CHECK_ERROR
        configs = (EGLConfig*) malloc(*nElements * sizeof(EGLConfig));
        if (eglGetConfigs(mGLDisplay, configs, *nElements, nElements) == EGL_FALSE)
        {
            OGRE_EXCEPT(Exception::ERR_RENDERINGAPI_ERROR,
                        "Failed to choose config",
                        __FUNCTION__);

            *nElements = 0;
            free(configs);
            return 0;
        }
        EGL_CHECK_ERROR
        return configs;
    }

    EGLBoolean EGLSupport::getGLConfigAttrib(EGLConfig glConfig, EGLint attribute, EGLint *value)
    {
        EGLBoolean status;

        status = eglGetConfigAttrib(mGLDisplay, glConfig, attribute, value);
        EGL_CHECK_ERROR
        return status;
    }

    void* EGLSupport::getProcAddress(const char* name) const
    {
        return (void*)eglGetProcAddress(name);
    }

    ::EGLConfig EGLSupport::getGLConfigFromContext(::EGLContext context)
    {
        ::EGLConfig glConfig = 0;

        if (eglQueryContext(mGLDisplay, context, EGL_CONFIG_ID, (EGLint *) &glConfig) == EGL_FALSE)
        {
            OGRE_EXCEPT(Exception::ERR_RENDERINGAPI_ERROR,
                        "Fail to get config from context",
                        __FUNCTION__);
            return 0;
        }
        EGL_CHECK_ERROR
        return glConfig;
    }

    ::EGLConfig EGLSupport::getGLConfigFromDrawable(::EGLSurface drawable,
                                                    unsigned int *w, unsigned int *h)
    {
        ::EGLConfig glConfig = 0;

        if (eglQuerySurface(mGLDisplay, drawable, EGL_CONFIG_ID, (EGLint *) &glConfig) == EGL_FALSE)
        {
            OGRE_EXCEPT(Exception::ERR_RENDERINGAPI_ERROR,
                        "Fail to get config from drawable",
                        __FUNCTION__);
            return 0;
        }
        EGL_CHECK_ERROR
        eglQuerySurface(mGLDisplay, drawable, EGL_WIDTH, (EGLint *) w);
        EGL_CHECK_ERROR
        eglQuerySurface(mGLDisplay, drawable, EGL_HEIGHT, (EGLint *) h);
        EGL_CHECK_ERROR
        return glConfig;
    }

    //------------------------------------------------------------------------
    // A helper class for the implementation of selectFBConfig
    //------------------------------------------------------------------------
    class GLConfigAttribs
    {
        public:
            GLConfigAttribs(const int* attribs)
            {
                fields[EGL_CONFIG_CAVEAT] = EGL_NONE;

                for (int i = 0; attribs[2*i] != EGL_NONE; i++)
                {
                    fields[attribs[2*i]] = attribs[2*i+1];
                }
            }

            void load(EGLSupport* const glSupport, EGLConfig glConfig)
            {
                std::map<int,int>::iterator it;

                for (it = fields.begin(); it != fields.end(); it++)
                {
                    it->second = EGL_NONE;

                    glSupport->getGLConfigAttrib(glConfig, it->first, &it->second);
                }
            }

            bool operator>(GLConfigAttribs& alternative)
            {
                // Caveats are best avoided, but might be needed for anti-aliasing
                if (fields[EGL_CONFIG_CAVEAT] != alternative.fields[EGL_CONFIG_CAVEAT])
                {
                    if (fields[EGL_CONFIG_CAVEAT] == EGL_SLOW_CONFIG)
                    {
                        return false;
                    }

                    if (fields.find(EGL_SAMPLES) != fields.end() &&
                        fields[EGL_SAMPLES] < alternative.fields[EGL_SAMPLES])
                    {
                        return false;
                    }
                }

                std::map<int,int>::iterator it;

                for (it = fields.begin(); it != fields.end(); it++)
                {
                    if (it->first != EGL_CONFIG_CAVEAT &&
                        fields[it->first] > alternative.fields[it->first])
                    {
                        return true;
                    }
                }

                return false;
            }

            std::map<int,int> fields;
    };

    ::EGLConfig EGLSupport::selectGLConfig(const int* minAttribs, const int *maxAttribs)
    {
        EGLConfig *glConfigs;
        EGLConfig glConfig = 0;
        int config, nConfigs = 0;

        glConfigs = chooseGLConfig(minAttribs, &nConfigs);

        if (!nConfigs)
        {
            glConfigs = getConfigs(&nConfigs);
        }

        if (!nConfigs)
        {
            return 0;
        }

        glConfig = glConfigs[0];

        if (maxAttribs)
        {
            GLConfigAttribs maximum(maxAttribs);
            GLConfigAttribs best(maxAttribs);
            GLConfigAttribs candidate(maxAttribs);

            best.load(this, glConfig);

            for (config = 1; config < nConfigs; config++)
            {
                candidate.load(this, glConfigs[config]);

                if (candidate > maximum)
                {
                    continue;
                }

                if (candidate > best)
                {
                    glConfig = glConfigs[config];

                    best.load(this, glConfig);
                }
            }
        }

        free(glConfigs);
        return glConfig;
    }

    void EGLSupport::switchMode(void)
    {
        return switchMode(mOriginalMode.first.first,
                          mOriginalMode.first.second, mOriginalMode.second);
    }

    NameValuePairList EGLSupport::parseOptions(uint& w, uint& h, bool& fullscreen)
    {
        ConfigOptionMap::iterator opt;
        ConfigOptionMap::iterator end = mOptions.end();
        NameValuePairList miscParams;

        fullscreen = false;
        w = 640, h = 480;

        if ((opt = mOptions.find("Full Screen")) != end)
        {
            fullscreen = (opt->second.currentValue == "Yes");
        }

        if ((opt = mOptions.find("Display Frequency")) != end)
        {
            miscParams["displayFrequency"] = opt->second.currentValue;
        }

        if ((opt = mOptions.find("Video Mode")) != end)
        {
            String val = opt->second.currentValue;
            String::size_type pos = val.find('x');

            if (pos != String::npos)
            {
                w = StringConverter::parseUnsignedInt(val.substr(0, pos));
                h = StringConverter::parseUnsignedInt(val.substr(pos + 1));
            }
        }

        if ((opt = mOptions.find("FSAA")) != end)
        {
            miscParams["FSAA"] = opt->second.currentValue;
        }

        if((opt = mOptions.find("VSync")) != end)
            miscParams["vsync"] = opt->second.currentValue;

        return miscParams;
    }

    ::EGLContext EGLSupport::createNewContext(EGLDisplay eglDisplay,
                          ::EGLConfig glconfig,
                                              ::EGLContext shareList) const 
    {
        EGLint contextAttrs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
        };

        if(mContextProfile != CONTEXT_ES) {
            if (!eglBindAPI(EGL_OPENGL_API))
            {
                OGRE_EXCEPT(Exception::ERR_RENDERINGAPI_ERROR,
                        "Couldn`t initialize API ",
                        "EGLSupport::getGLDisplay");
            }
            EGL_CHECK_ERROR

            contextAttrs[0] = EGL_NONE;
        }

        ::EGLContext context = 0;
        if (!eglDisplay)
        {
            context = eglCreateContext(mGLDisplay, glconfig, shareList, contextAttrs);
            EGL_CHECK_ERROR
        }
        else
        {
            context = eglCreateContext(eglDisplay, glconfig, 0, contextAttrs);
            EGL_CHECK_ERROR
        }

        if (!context)
        {
            OGRE_EXCEPT(Exception::ERR_RENDERINGAPI_ERROR,
                        "Fail to create New context",
                        __FUNCTION__);
            return 0;
        }

        return context;
    }

    void EGLSupport::start()
    {
        LogManager::getSingleton().logMessage(
            "******************************\n"
            "*** Starting EGL Subsystem ***\n"
            "******************************");
        initialiseExtensions();
    }

    void EGLSupport::stop()
    {
        eglTerminate(mGLDisplay);
        EGL_CHECK_ERROR
    }

    void EGLSupport::initialiseExtensions() {
        assert (mGLDisplay);

        const char* verStr = eglQueryString(mGLDisplay, EGL_VERSION);
        LogManager::getSingleton().stream() << "EGL_VERSION = " << verStr;

        const char* extensionsString;

        // This is more realistic than using glXGetClientString:
        extensionsString = eglQueryString(mGLDisplay, EGL_EXTENSIONS);

        LogManager::getSingleton().stream() << "EGL_EXTENSIONS = " << extensionsString;

        StringStream ext;
        String instr;

        ext << extensionsString;

        while(ext >> instr)
        {
            extensionList.insert(instr);
        }
    }

    void EGLSupport::setGLDisplay( EGLDisplay val )
    {
        mGLDisplay = val;
    }
}
