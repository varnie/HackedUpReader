#include "readerview.h"
#include "lvdocview.h"

static jfieldID gNativeObjectID = 0;

class DocViewCallback : public LVDocViewCallback {
	CRJNIEnv _env;
	LVDocView * _docview;
	jclass _class;
	jobject _obj;
	jmethodID _OnLoadFileStart;
    jmethodID _OnLoadFileFormatDetected;
    jmethodID _OnLoadFileEnd;
    jmethodID _OnLoadFileFirstPagesReady;
    jmethodID _OnLoadFileProgress;
    jmethodID _OnFormatStart;
    jmethodID _OnFormatEnd;
    jmethodID _OnFormatProgress;
    jmethodID _OnExportProgress;
    jmethodID _OnLoadFileError;
    jmethodID _OnExternalLink;
public:
	DocViewCallback( JNIEnv * env, LVDocView * docview, jobject obj )
	: _env(env), _docview(docview)
	{
		jclass objclass = _env->GetObjectClass(obj);
		jfieldID fid = _env->GetFieldID(objclass, "readerCallback", "Lorg/coolreader/crengine/ReaderCallback;");
		_obj = _env->GetObjectField(obj, fid); 
		_class = _env->GetObjectClass(_obj);
		#define GET_METHOD(n,sign) \
		     _ ## n = _env->GetMethodID(_class, # n, sign)   
		GET_METHOD(OnLoadFileStart,"(Ljava/lang/String;)V");
	    GET_METHOD(OnLoadFileFormatDetected,"(Lorg/coolreader/crengine/DocumentFormat;)Ljava/lang/String;");
	    GET_METHOD(OnLoadFileEnd,"()V");
	    GET_METHOD(OnLoadFileFirstPagesReady,"()V");
	    GET_METHOD(OnLoadFileProgress,"(I)Z");
	    GET_METHOD(OnFormatStart,"()V");
	    GET_METHOD(OnFormatEnd,"()V");
	    GET_METHOD(OnFormatProgress,"(I)Z");
	    GET_METHOD(OnExportProgress,"(I)Z");
	    GET_METHOD(OnLoadFileError,"(Ljava/lang/String;)V");
	    GET_METHOD(OnExternalLink,"(Ljava/lang/String;Ljava/lang/String;)V");
		_docview->setCallback( this );
	}
	virtual ~DocViewCallback()
	{
		_docview->setCallback( NULL );
	}
    /// on starting file loading
    virtual void OnLoadFileStart( lString16 filename )
    {
		CRLog::info("DocViewCallback::OnLoadFileStart() called");
    	_env->CallVoidMethod(_obj, _OnLoadFileStart, _env.toJavaString(filename));
    }
    /// format detection finished
    virtual void OnLoadFileFormatDetected( doc_format_t fileFormat )
    {
		CRLog::info("DocViewCallback::OnLoadFileFormatDetected() called");
    	jobject e = _env.enumByNativeId("org/coolreader/crengine/DocumentFormat", (int)fileFormat);
    	jstring css = (jstring)_env->CallObjectMethod(_obj, _OnLoadFileFormatDetected, e);
    	if ( css ) {
    		lString16 s = _env.fromJavaString(css);
    		CRLog::info("OnLoadFileFormatDetected: setting CSS for format %d", (int)fileFormat);
    		_docview->setStyleSheet( UnicodeToUtf8(s) );
    	}
    }
    /// file loading is finished successfully - drawCoveTo() may be called there
    virtual void OnLoadFileEnd()
    {
		CRLog::info("DocViewCallback::OnLoadFileEnd() called");
    	_env->CallVoidMethod(_obj, _OnLoadFileEnd);
    }
    /// first page is loaded from file an can be formatted for preview
    virtual void OnLoadFileFirstPagesReady()
    {
		CRLog::info("DocViewCallback::OnLoadFileFirstPagesReady() called");
    	_env->CallVoidMethod(_obj, _OnLoadFileFirstPagesReady);
    }
    /// file progress indicator, called with values 0..100
    virtual void OnLoadFileProgress( int percent )
    {
		CRLog::info("DocViewCallback::OnLoadFileProgress() called");
    	jboolean res = _env->CallBooleanMethod(_obj, _OnLoadFileProgress, (jint)(percent*100));
    }
    /// document formatting started
    virtual void OnFormatStart()
    {
		CRLog::info("DocViewCallback::OnFormatStart() called");
    	_env->CallVoidMethod(_obj, _OnFormatStart);
    }
    /// document formatting finished
    virtual void OnFormatEnd()
    {
		CRLog::info("DocViewCallback::OnFormatEnd() called");
    	_env->CallVoidMethod(_obj, _OnFormatEnd);
    }
    /// format progress, called with values 0..100
    virtual void OnFormatProgress( int percent )
    {
		CRLog::info("DocViewCallback::OnFormatProgress() called");
    	jboolean res = _env->CallBooleanMethod(_obj, _OnFormatProgress, (jint)(percent*100));
    }
    /// format progress, called with values 0..100
    virtual void OnExportProgress( int percent )
    {
		CRLog::info("DocViewCallback::OnExportProgress() called");
    	jboolean res = _env->CallBooleanMethod(_obj, _OnExportProgress, (jint)(percent*100));
    }
    /// file load finiished with error
    virtual void OnLoadFileError( lString16 message )
    {
		CRLog::info("DocViewCallback::OnLoadFileError() called");
    	_env->CallVoidMethod(_obj, _OnLoadFileError, _env.toJavaString(message));
    }
    /// Override to handle external links
    virtual void OnExternalLink( lString16 url, ldomNode * node )
    {
		CRLog::info("DocViewCallback::OnExternalLink() called");
    	lString16 path = ldomXPointer(node,0).toString();
    	_env->CallVoidMethod(_obj, _OnExternalLink, _env.toJavaString(url), _env.toJavaString(path));
    }
};


ReaderViewNative::ReaderViewNative()
{
	_docview = new LVDocView(32); //32bpp
	_docview->setFontSize(24);
	_docview->createDefaultDocument(lString16("Welcome to CoolReader"), lString16("Please select file to open"));
}

static ReaderViewNative * getNative(JNIEnv * env, jobject _this)
{
    return (ReaderViewNative *)env->GetIntField(_this, gNativeObjectID); 
}

bool ReaderViewNative::loadDocument( lString16 filename )
{
	CRLog::info("Loading document %s", LCSTR(filename));
	bool res = _docview->LoadDocument(filename.c_str());
	CRLog::info("Document %s is loaded %s", LCSTR(filename), (res?"successfully":"with error"));
    return res;
}

bool ReaderViewNative::openRecentBook()
{
	CRLog::debug("ReaderViewNative::openRecentBook()");
	int index = 0;
	if ( _docview->isDocumentOpened() ) {
		CRLog::debug("ReaderViewNative::openRecentBook() : saving previous document state");
		_docview->swapToCache();
        _docview->getDocument()->updateMap();
	    _docview->savePosition();
		closeBook();
	    index = 1;
	}
    LVPtrVector<CRFileHistRecord> & files = _docview->getHistory()->getRecords();
    CRLog::info("ReaderViewNative::openRecentBook() : %d files found in history, startIndex=%d", files.length(), index);
    if ( index < files.length() ) {
        CRFileHistRecord * file = files.get( index );
        lString16 fn = file->getFilePathName();
        CRLog::info("ReaderViewNative::openRecentBook() : checking file %s", LCSTR(fn));
        // TODO: check error
        if ( LVFileExists(fn) ) {
            return loadDocument( fn );
        } else {
        	CRLog::error("file %s doesn't exist", LCSTR(fn));
        	return false;
        }
        //_docview->swapToCache();
    } else {
        CRLog::info("ReaderViewNative::openRecentBook() : no recent book found in history");
    }
    return false;
}

bool ReaderViewNative::closeBook()
{
	if ( _docview->isDocumentOpened() ) {
	    _docview->savePosition();
        _docview->getDocument()->updateMap();
	    saveHistory(lString16());
	    _docview->close();
	    return true;
	}
	return false;
}

bool ReaderViewNative::loadHistory( lString16 filename )
{
    CRFileHist * hist = _docview->getHistory();
	if ( !filename.empty() )
		historyFileName = filename;
    historyFileName = filename;
    if ( historyFileName.empty() ) {
    	CRLog::error("No history file name specified");
    	return false;
    }
	CRLog::info("Trying to load history from file %s", LCSTR(historyFileName));
    LVStreamRef stream = LVOpenFileStream(historyFileName.c_str(), LVOM_READ);
    if ( stream.isNull() ) {
    	CRLog::error("Cannot open file %s", LCSTR(historyFileName));
    	return false;
    }
    bool res = hist->loadFromStream( stream );
    if ( res )
    	CRLog::info("%d items found", hist->getRecords().length());
    else
    	CRLog::error("Cannot read history file content");
    return res;
}

bool ReaderViewNative::saveHistory( lString16 filename )
{
	if ( !filename.empty() )
		historyFileName = filename;
    if ( historyFileName.empty() )
    	return false;
	if ( _docview->isDocumentOpened() ) {
		CRLog::debug("ReaderViewNative::saveHistory() : saving position");
	    _docview->savePosition();
	}
	CRLog::info("Trying to save history to file %s", LCSTR(historyFileName));
    CRFileHist * hist = _docview->getHistory();
    LVStreamRef stream = LVOpenFileStream(historyFileName.c_str(), LVOM_WRITE);
    if ( stream.isNull() ) {
    	CRLog::error("Cannot create file %s for writing", LCSTR(historyFileName));
    	return false;
    }
    if ( _docview->isDocumentOpened() )
    	_docview->savePosition();
    return hist->saveToStream( stream.get() );
}

int ReaderViewNative::doCommand( int cmd, int param )
{
	switch (cmd) {
    case DCMD_OPEN_RECENT_BOOK:
    	{
    		return openRecentBook();
    	}
    	break;
    case DCMD_CLOSE_BOOK:
    	{
    		return closeBook();
    	}
    	break;
    case DCMD_RESTORE_POSITION:
	    {
    		if ( _docview->isDocumentOpened() ) {
		        _docview->restorePosition();
    		}
	    }
	    break;
    default:
    	return 0;
   	}
   	return 1;
}


/*
 * Class:     org_coolreader_crengine_ReaderView
 * Method:    createInternal
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_coolreader_crengine_ReaderView_createInternal
  (JNIEnv * env, jobject _this)
{
	CRLog::info("Creating new RenderView");
    jclass rvClass = env->FindClass("org/coolreader/crengine/ReaderView");
    gNativeObjectID = env->GetFieldID(rvClass, "mNativeObject", "I");
    ReaderViewNative * obj = new ReaderViewNative();
    env->SetIntField(_this, gNativeObjectID, (jint)obj);
    obj->_docview->setFontSize(24); 
}

/*
 * Class:     org_coolreader_crengine_ReaderView
 * Method:    destroyInternal
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_coolreader_crengine_ReaderView_destroyInternal
  (JNIEnv * env, jobject view)
{
    ReaderViewNative * p = getNative(env, view);
    if ( p!=NULL ) {
		CRLog::info("Destroying RenderView");
    	delete p;
	    jclass rvClass = env->FindClass("org/coolreader/crengine/ReaderView");
	    gNativeObjectID = env->GetFieldID(rvClass, "mNativeObject", "I");
	    env->SetIntField(view, gNativeObjectID, 0);
	} else {
		CRLog::error("RenderView is already destroyed");
	}
}

/*
 * Class:     org_coolreader_crengine_ReaderView
 * Method:    getPageImage
 * Signature: (Landroid/graphics/Bitmap;)V
 */
JNIEXPORT void JNICALL Java_org_coolreader_crengine_ReaderView_getPageImage
  (JNIEnv * env, jobject view, jobject bitmap)
{
    ReaderViewNative * p = getNative(env, view);
    //CRLog::info("Initialize callback");
	DocViewCallback callback( env, p->_docview, view );	
    //CRLog::info("Initialized callback");
	BitmapAccessor bmp(env,bitmap);
	if ( bmp.isOk() ) {
	    LVDocImageRef img = p->_docview->getPageImage(0);
	    LVDrawBuf * drawbuf = img->getDrawBuf();
	    bmp.draw( drawbuf, 0, 0 );
	    
//		int dx = bmp.getWidth();
		//int dy = bmp.getHeight();
		//LVColorDrawBuf buf( dx, dy );
		//buf.FillRect(0, 0, dx, dy, 0xFF8000);
		//buf.FillRect(10, 10, dx-20, dy-20, 0xC0C0FF);
		//buf.FillRect(20, 20, 30, 30, 0x0000FF);
		//LVFontRef fnt = fontMan->GetFont(30, 400, false, css_ff_sans_serif, lString8("Droid Sans"));
		//fnt->DrawTextString(&buf, 40, 40, L"Text 1", 6, '?', NULL, false, 0, 0);
		//fnt->DrawTextString(&buf, 40, 90, L"Text 2", 6, '?', NULL, false, 0, 0);
		//bmp.draw( &buf, 0, 0 );		
//		lUInt32 row[500];
//		for ( int y=0; y<300; y++ ) {
//			for ( int x=0; x<500; x++ ) {
//				row[x] = x*5 + y*30;
//			}
//			bmp.setRowRGB( 0, y, row, 500 );
//		}
	} else {
		CRLog::error("bitmap accessor is invalid");
	}
}

/*
 * Class:     org_coolreader_crengine_ReaderView
 * Method:    loadDocument
 * Signature: (Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_org_coolreader_crengine_ReaderView_loadDocumentInternal
  (JNIEnv * _env, jobject _this, jstring s)
{
	CRJNIEnv env(_env);
    ReaderViewNative * p = getNative(_env, _this);
	DocViewCallback callback( _env, p->_docview, _this );
	lString16 str = env.fromJavaString(s);
    bool res = p->loadDocument(str);
    return res ? JNI_TRUE : JNI_FALSE;
}

/*
 * Class:     org_coolreader_crengine_ReaderView
 * Method:    getSettings
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_coolreader_crengine_ReaderView_getSettingsInternal
  (JNIEnv *, jobject)
{
    return NULL;
}

/*
 * Class:     org_coolreader_crengine_ReaderView
 * Method:    applySettings
 * Signature: (Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_org_coolreader_crengine_ReaderView_applySettingsInternal
  (JNIEnv *, jobject, jstring)
{
    return false;
}

/*
 * Class:     org_coolreader_crengine_ReaderView
 * Method:    readHistory
 * Signature: (Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_org_coolreader_crengine_ReaderView_readHistoryInternal
  (JNIEnv * _env, jobject _this, jstring jFilename)
{
	CRJNIEnv env(_env);
    ReaderViewNative * p = getNative(_env, _this);
    bool res = p->loadHistory( env.fromJavaString(jFilename) );
    return res?JNI_TRUE:JNI_FALSE;
}

/*
 * Class:     org_coolreader_crengine_ReaderView
 * Method:    writeHistory
 * Signature: (Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_org_coolreader_crengine_ReaderView_writeHistoryInternal
  (JNIEnv * _env, jobject _this, jstring jFilename)
{
	CRJNIEnv env(_env);
    ReaderViewNative * p = getNative(_env, _this);
    bool res = p->saveHistory( env.fromJavaString(jFilename) );
    return res?JNI_TRUE:JNI_FALSE;
}

/*
 * Class:     org_coolreader_crengine_ReaderView
 * Method:    setStylesheet
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_coolreader_crengine_ReaderView_setStylesheetInternal
  (JNIEnv * _env, jobject _view, jstring jcss)
{
	CRJNIEnv env(_env);
    ReaderViewNative * p = getNative(_env, _view);
    lString8 css8 = UnicodeToUtf8(env.fromJavaString(jcss));
    p->_docview->setStyleSheet(css8);
}

/*
 * Class:     org_coolreader_crengine_ReaderView
 * Method:    resize
 * Signature: (II)V
 */
JNIEXPORT void JNICALL Java_org_coolreader_crengine_ReaderView_resizeInternal
  (JNIEnv * _env, jobject _this, jint dx, jint dy)
{
    ReaderViewNative * p = getNative(_env, _this);
    p->_docview->Resize(dx, dy);
}  
  

/*
 * Class:     org_coolreader_crengine_ReaderView
 * Method:    doCommand
 * Signature: (II)Z
 */
JNIEXPORT jboolean JNICALL Java_org_coolreader_crengine_ReaderView_doCommandInternal
  (JNIEnv * _env, jobject _this, jint cmd, jint param)
{
    ReaderViewNative * p = getNative(_env, _this);
	DocViewCallback callback( _env, p->_docview, _this );	
    if ( cmd>=READERVIEW_DCMD_START && cmd<=READERVIEW_DCMD_END) {
    	return p->doCommand(cmd, param)?JNI_TRUE:JNI_FALSE;
    }
    
    p->_docview->doCommand((LVDocCmd)cmd, param);
    return JNI_TRUE;
}

/*
 * Class:     org_coolreader_crengine_ReaderView
 * Method:    getState
 * Signature: ()Lorg/coolreader/crengine/ReaderView/DocumentInfo;
 */
JNIEXPORT jobject JNICALL Java_org_coolreader_crengine_ReaderView_getStateInternal
  (JNIEnv *, jobject)
{
    return NULL;
}
  
