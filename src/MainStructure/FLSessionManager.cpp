//
//  FaustLiveApp.cpp
//
//  Created by Sarah Denoux on 12/04/13.
//  Copyright (c) 2013 __MyCompanyName__. All rights reserved.
//

#include "FLSessionManager.h"

#include "FLSettings.h"
#include "FLWinSettings.h"
#include "utilities.h"

#include "FLErrorWindow.h"

//#include "FLrenameDialog.h"

#include "faust/llvm-dsp.h"
#ifdef REMOTE
#include "faust/remote-dsp.h"
#endif

#define DEFAULTNAME "DefaultName"
#define kMaxSHAFolders 100


FLSessionManager* FLSessionManager::_sessionManager = 0;

//----------------------CONSTRUCTOR/DESTRUCTOR---------------------------
FLSessionManager::FLSessionManager(const QString& sessionFolder){
    fSessionFolder = sessionFolder;
}

FLSessionManager::~FLSessionManager(){}

FLSessionManager* FLSessionManager::_Instance(){
    return FLSessionManager::_sessionManager;
}

void FLSessionManager::createInstance(const QString homePath){
    FLSessionManager::_sessionManager = new FLSessionManager(homePath);
}

void FLSessionManager::deleteInstance(){
    delete FLSessionManager::_sessionManager;
}

//---Managing SHAFolder content. SHAFolder modified date is updated whenever a file of the folder is used. Then when there are too many folder saved, the most former used folder is deleted
void FLSessionManager::updateFolderDate(const QString& shaValue){
    
    QString shaFolder = fSessionFolder + "/SHAFolder/" + shaValue;
    touchFolder(shaFolder);
}

void FLSessionManager::cleanSHAFolder(){
    QString shaFolder = fSessionFolder + "/SHAFolder";
    
    QDir shaDir(shaFolder);
    
    QFileInfoList children = shaDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
    
    if(children.size() > kMaxSHAFolders){
        
        for(QFileInfoList::iterator it = children.begin(); it != children.end(); it++){
//            printf("DATE OF CHILDREN = %s\n", it->lastModified().toString().toStdString().c_str());
            
            if(it == (children.end()-1)){
                
                QString childToDelete = it->absoluteFilePath();
                deleteDirectoryAndContent(childToDelete);
            }
        }
    }
        
}

//Default Names
QList<QString> FLSessionManager::get_currentDefault(){
    
    QList<QString> currentDefault;
    
    FLSettings::_Instance()->beginGroup("Windows");
    QStringList groups  = FLSettings::_Instance()->childKeys();
    
    for(int i=0; i<groups.size(); i++){
        
        QString settingPath = QString::number(i) + "/Name";
        
        QString settingName = FLSettings::_Instance()->value(settingPath, "").toString();
        
        if(settingName.indexOf(DEFAULTNAME) != -1){
            currentDefault.push_back(settingName);
        }
    }
    FLSettings::_Instance()->endGroup();
    
    return currentDefault;
}

QString FLSessionManager::find_smallest_defaultName(){
    
    //Conditional jump on currentDefault List...
    
    int index = 1;
    QString nomEffet("");
    bool found = false;
    
    QList<QString> currentDefault = get_currentDefault();
    
    do{
        nomEffet = DEFAULTNAME;
        nomEffet += "_";
        nomEffet += QString::number(index);
        
        QList<QString>::iterator it;
        
        for(it = currentDefault.begin(); it != currentDefault.end(); it++){
            if(nomEffet.compare(*it) == 0){
                found = true;
                break;
            }
            else
                found = false;
        }
        index++;
        
    }while(found);
    
    return nomEffet;
}

//Return declare name if there is one.
//If the name exists in the session, the user is asked to choose another one
QString FLSessionManager::getDeclareName(QString text){
    
    QString returning = "";
    int pos = text.indexOf("declare name");
    
    if(pos != -1){
        text.remove(0, pos);
        
        pos=text.indexOf("\"");
        if(pos != -1){
            text.remove(0, pos+1);
        }
        pos = text.indexOf("\"");
        text.remove(pos, text.length()-pos);
        
        returning = text;
    }
    
    return returning;
}

// Transforming DSP Name to make sure it is unique in the session
QString FLSessionManager::nameToUniqueName(const QString& name, const QString& path){
    
    QString newName(name);
    
    FLSettings* generalSettings = FLSettings::_Instance();
    
    generalSettings->beginGroup("Windows");
    QStringList groups  = generalSettings->childKeys();
    
    for(int i=0; i<groups.size(); i++){
        
        QString settingPath = groups[i] + "/Path";
        QString settingName = groups[i] + "/Name";
        
        QString tempPath = generalSettings->value(settingPath, "").toString();
        QString tempName = generalSettings->value(settingName, "").toString();
        /*
        while(tempName == name && path != "" && path != tempPath){
            
            FLrenameDialog* Msg = new FLrenameDialog(newName, 0);
            Msg->raise();
            Msg->exec();
            newName = Msg->getNewName();
            
            while(newName.indexOf(' ') != -1)
                newName.remove(newName.indexOf(' '), 1);
            
            break;
        }*/
    }
    generalSettings->endGroup();
    
    return newName;
}

bool FLSessionManager::isSourceDSPPath(const QString& source){
    
    if(QFileInfo(source).exists() && QFileInfo(source).completeSuffix() == "dsp")
        return true;
    else
        return false;
}

QString FLSessionManager::ifFileToName(const QString& source){
    
    if(isSourceDSPPath(source))
        return QFileInfo(source).baseName();
    else
        return "";
}

//--Transforms DSP file into faust string
QString FLSessionManager::ifFileToString(const QString& source){

    if(isSourceDSPPath(source)){
        return pathToContent(source);
    }
    else
        return source;
}

//--Transforms an Url into faust string
QString FLSessionManager::ifUrlToString(const QString& source){
    
    //In case the text dropped is a web url
    int pos = source.indexOf("http://");
    
    QString UrlText(source);
    
    //    Has to be at the beginning, otherwise, it can be a component containing an URL.
    if(pos == 0){
        UrlText = "process = component(\"";
        UrlText += source;
        UrlText +="\");";
    }
    
    return UrlText;
}

//--Transforms Wav file into faust string
QString FLSessionManager::ifWavToString(const QString& source){
    //    --> à voir comment on gère, vu qu'on enregistre pas de fichier source "intermédiaire". Est-ce qu'on recalcule la waveform quand on demande d'éditer ??
    
    if(QFileInfo(source).completeSuffix() == "wav"){
        
        QString soundFileName = QFileInfo(source).baseName();
        
        QString destinationFile = QFileInfo(source).absolutePath();
        destinationFile += "/" ;
        destinationFile += soundFileName;
        
        QString waveFile = destinationFile;
        waveFile += "_waveform.dsp";
        
        destinationFile += ".dsp";
        
        QString systemInstruct;

		// figure out the right name for faust2sound depending of the OS

#ifdef _WIN32
        systemInstruct += "sound2faust.exe ";
#endif

#ifdef __linux__
        if(QFileInfo("/usr/local/bin/sound2faust").exists())
            systemInstruct += "/usr/local/bin/sound2faust ";
        else
            systemInstruct += "./sound2faust ";      
#endif

#ifdef __APPLE__
        QDir base;
        
        if(base.absolutePath().indexOf("Contents/MacOS") != -1)
            systemInstruct += "./sound2faust ";
        else
            systemInstruct += base.absolutePath() + "/FaustLive.app/Contents/MacOs/sound2faust ";      
#endif
        systemInstruct += "\"" + source + "\"" + " -o " + waveFile;
        
        QString errorMsg("");
        if(!executeInstruction(systemInstruct, errorMsg))
            emit error(errorMsg);
        
        QString finalFileContent = "import(\"";
        finalFileContent += soundFileName + "_waveform.dsp";
        finalFileContent += "\");\nprocess=";
        finalFileContent += QFileInfo(source).baseName();
        finalFileContent += ";";
        
        writeFile(destinationFile, finalFileContent);
        
        return destinationFile;
    }
    else
        return source;
}

//--Fill argv parameters with -I/-O/etc...
const char** FLSessionManager::getFactoryArgv(const QString& sourcePath, const QString& destPath, const QString& faustOptions, int& argc){
    
    //--------Compilation Options 
    
    int numberFixedParams = 7;
    
    if(sourcePath == "")
        numberFixedParams = numberFixedParams-2;
    
    int iteratorParams = 0;
    
#ifdef _WIN32
    numberFixedParams = numberFixedParams+2;
#endif
    
    //+7 = -I libraryPath -I currentFolder -O drawPath -svg
    argc = numberFixedParams;
    argc += get_numberParameters(faustOptions);
    
    const char** argv = new const char*[argc];
    
    argv[iteratorParams] = "-I";
    iteratorParams++;
    
    //The library path is where libraries like the scheduler architecture file are = currentSession
    string libsFolder = fSessionFolder.toStdString() + "/Libs";
    
    string libPath = libsFolder;
    char* libP = new char[libsFolder.size()+1];
    strncpy( libP, libPath.c_str(), libsFolder.size()+1);
    argv[iteratorParams] = (const char*) libP;
    iteratorParams++;

    if(sourcePath != ""){
        argv[iteratorParams] = "-I";   
        iteratorParams++;
        
        QString sourceChemin = QFileInfo(sourcePath).absolutePath();
        string path = sourceChemin.toStdString();
        
        char* libP2 = new char[sourceChemin.size()+1];
        strncpy( libP2, path.c_str(), sourceChemin.size()+1);
        argv[iteratorParams] = (const char*) libP2;
        
        iteratorParams++;
    }
    
    argv[iteratorParams] = "-O";
    iteratorParams++;
    
    QDir direct(destPath);
    direct.mkdir(destPath);
    
    string pathSVG = destPath.toStdString();
    
    char* svgP = new char[pathSVG.size()+1];
    strncpy( svgP, pathSVG.c_str(), pathSVG.size()+1);
    argv[iteratorParams] = (const char*) svgP;
    
    iteratorParams++;
    argv[iteratorParams] = "-svg";
    iteratorParams++;
    
#ifdef _WIN32
    //LLVM_MATH is added to resolve mathematical float functions, like powf
    argv[iteratorParams] = "-l";
    iteratorParams++;
    argv[iteratorParams] = "llvm_math.ll";
    iteratorParams++;
#endif
    
    //Parsing the compilationOptions from a string to a char**
    QString copy = faustOptions;
    
    for(int i=numberFixedParams; i<argc; i++){
        
        string parseResult(parse_compilationParams(copy));
        
        char* intermediate = new char[parseResult.size()+1];
        
        strcpy(intermediate,parseResult.c_str());
        
        //        OPTION DOUBLE HAS TO BE SKIPED, it causes segmentation fault
        if(strcmp(intermediate, "-double") != 0)
            argv[i] = (const char*)intermediate;
        else{
            argc--;
            i--;
        }
    }

    return argv;
}

const char** FLSessionManager::getRemoteInstanceArgv(QSettings* winSettings, int& argc){
    
    argc = 8;
    const char** argv = new const char*[argc];
    
    argv[0] = "--NJ_ip";
    QString localString = searchLocalIP();
    string ip(localString.toStdString());
    
    char* ipString = new char[ip.size()+1];
    strncpy( ipString, ip.c_str(), ip.size()+1);
    argv[1] = (const char*) ipString;
    
    argv[2] = "--NJ_latency";
    QString latency = winSettings->value("RemoteProcessing/Latency", "10").toString();
    string lat = latency.toStdString();
    
    char* latString = new char[lat.size()+1];
    strncpy( latString, lat.c_str(), lat.size()+1);
    argv[3] = (const char*) latString;

    
    argv[4] = "--NJ_compression";
    QString cv = winSettings->value("RemoteProcessing/CV", "64").toString();
    string compression = cv.toStdString();
    char* cvString = new char[compression.size()+1];
    strncpy( cvString, compression.c_str(), compression.size()+1);
    argv[5] = (const char*) cvString;
    
    argv[6] = "--NJ_mtu";
    QString mtu = winSettings->value("RemoteProcessing/MTU", "1500").toString();
    string mtuVal = mtu.toStdString();
    char* mtuString = new char[mtuVal.size()+1];
    strncpy( mtuString, mtuVal.c_str(), mtuVal.size()+1);
    argv[7] = (const char*) mtuString;
    
    return argv;
}

void    FLSessionManager::deleteArgv(int argc, const char** argv){
    
    for(int i=0; i<argc; i++)
        delete argv[i];
    
    delete[] argv;
}

QString FLSessionManager::getErrorFromCode(int code){
    
#ifdef REMOTE
    if(code == ERROR_FACTORY_NOTFOUND){
        return "Impossible to create remote factory";
    }
    
    if(code == ERROR_INSTANCE_NOTCREATED){
        return "Impossible to create DSP Instance";
    }
    else if(code == ERROR_NETJACK_NOTSTARTED){
        return "NetJack Master not started";
    }
    else if (code == ERROR_CURL_CONNECTION){
        return "Curl connection failed";
    }
#endif
    
    return "ERROR not recognized";
}

bool    FLSessionManager::generateAuxFiles(const std::string& shaKey, FLWinSettings* settings){
    
    QString faustOptions = settings->value("AutomaticExport/Options", "").toString();

    int argc = get_numberParameters(faustOptions);
        
    const char** argv = new const char*[argc];
        
    QString copy = faustOptions;
        
    for(int i=0; i<argc; i++){
            
        string parseResult = parse_compilationParams(copy);
            
        char* intermediate = new char[parseResult.size()+1];
        
        strcpy(intermediate,parseResult.c_str());
            
        argv[i] = (const char*)intermediate;
        printf("ARGV = %s\n", argv[i]);
    }

    string error;
    string sourceFile = fSessionFolder.toStdString() + "/SHAFolder/" + shaKey + "/" + shaKey + ".dsp";

    printf("generate Aux Files From file : %s\n", sourceFile.c_str());
        
    generateAuxFilesFromFile(sourceFile, argc, argv, error);
}

QPair<QString, void*> FLSessionManager::createFactory(const QString& source, FLWinSettings* settings, QString& errorMsg){
    //-------Clean Factory Folder if needed
    cleanSHAFolder();
    
    //-------Get Faust Code
    
    QString faustContent = ifUrlToString(source);
    faustContent = ifWavToString(faustContent);
    
    //Path is whether the dsp source unmodified or the waveform converted
    QString path("");
    if(isSourceDSPPath(faustContent))
        path = faustContent;
    
    faustContent = ifFileToString(faustContent);
    
    //------Get Name
    
    QString name = ifFileToName(path);
    
    if(name == "")
        name = getDeclareName(faustContent);
    
    if(name == "")
        name = find_smallest_defaultName();
    
    //name = nameToUniqueName(name, path);
    
    //--------Calculation of SHA Key
    
//-----Extracting compilation Options from general options Or window options
    QString defaultOptions = FLSettings::_Instance()->value("General/Compilation/FaustOptions", "").toString();
    int defaultOptLevel = FLSettings::_Instance()->value("General/Compilation/OptValue", 3).toInt();
    
    QString faustOptions = defaultOptions;
    int optLevel = defaultOptLevel;
    
    if(settings){
        faustOptions = settings->value("Compilation/FaustOptions", defaultOptions).toString();
        optLevel = settings->value("Compilation/OptValue", defaultOptLevel).toInt();
    }
    
	string organizedOptions = FL_reorganize_compilation_options(faustOptions);
    
    string optvalue = QString::number(optLevel).toStdString();
    
    string fullShaString = organizedOptions + optvalue + faustContent.toStdString();
    
    string shaKey = FL_generate_sha1(fullShaString);
    
    QString factoryFolder = fSessionFolder + "/SHAFolder/" + shaKey.c_str();
    
    string irFile = factoryFolder.toStdString() + "/" + shaKey;
    
    QString faustFile = factoryFolder + "/" + shaKey.c_str() + ".dsp";
    
    if(!QFileInfo(irFile.c_str()).exists()){
        QDir newFolder(factoryFolder);
        newFolder.mkdir(factoryFolder);
        
        writeFile(faustFile, faustContent);
    }

    string fileToCompile(faustFile.toStdString());
    string nameToCompile(name.toStdString());
  
	//----CreateFactory

    factorySettings* mySetts = new factorySettings;
    factory* toCompile = new factory;
    string error;
	
    int argc;
	const char** argv = getFactoryArgv(path, factoryFolder, faustOptions, argc);
    
    QString machineName = "local processing";
    
    if(settings)
        machineName = settings->value("RemoteProcessing/MachineName", machineName).toString();
    
    generateAuxFiles(shaKey, settings);
    
    if(machineName == "local processing"){
       mySetts->fType = TYPE_LOCAL;
        
        //----Use IR Saving if possible
        if(QFileInfo(irFile.c_str()).exists())
            toCompile->fLLVMFactory = readDSPFactoryFromBitcodeFile(irFile, "", 0);

        //----Create DSP Factory
        if(toCompile->fLLVMFactory == NULL){
            
            toCompile->fLLVMFactory = createDSPFactoryFromFile(fileToCompile, argc, argv, "", error, optLevel);
            
            if(settings){
                settings->setValue("InputNumber", 0);
                settings->setValue("OutputNumber", 0);
            }
            
            if(toCompile->fLLVMFactory)
                writeDSPFactoryToBitcodeFile(toCompile->fLLVMFactory, irFile);
            else{
                errorMsg = error.c_str();
			    return qMakePair(QString(""), (void*)NULL);
            }
        }
    }
    else if(settings){
#ifdef REMOTE
        mySetts->fType = TYPE_REMOTE;
        
        string ip_server = settings->value("RemoteProcessing/MachineIP", "127.0.0.1").toString().toStdString();
        int port_server = settings->value("RemoteProcessing/MachinePort", 7777).toInt();
        
        toCompile->fRemoteFactory = createRemoteDSPFactoryFromFile( fileToCompile, argc, argv, ip_server, port_server, error, optLevel);
        
        if(!toCompile->fRemoteFactory){
            errorMsg = error.c_str();
            return qMakePair(QString(""), (void*)NULL);
        }
        
        settings->setValue("InputNumber", toCompile->fRemoteFactory->numInputs());
        settings->setValue("OutputNumber", toCompile->fRemoteFactory->numOutputs());
#endif
    }

    mySetts->fFactory = toCompile;
    mySetts->fPath = path;
    mySetts->fName = name;

   return qMakePair(QString(shaKey.c_str()), (void*)(mySetts));
}

dsp* FLSessionManager::createDSP(QPair<QString, void*> factorySetts, const QString& source, FLWinSettings* settings, RemoteDSPErrorCallback error_callback, void* error_callback_arg, QString& errorMsg){
    
    factorySettings* mySetts = (factorySettings*)(factorySetts.second);
    
    factory* toCompile = mySetts->fFactory;
    
    QString path = mySetts->fPath;
    QString name = mySetts->fName;
    int type = mySetts->fType;
    
    dsp* compiledDSP = NULL;
    
    if(type == TYPE_LOCAL){
        
        //----Create DSP Instance
        compiledDSP = createDSPInstance(toCompile->fLLVMFactory);
		if(compiledDSP == NULL)
            errorMsg = "Impossible to compile DSP";
    }
#ifdef REMOTE
    else if(settings){
        int sampleRate = settings->value("SampleRate", 44100).toInt();
        int bufferSize = settings->value("BufferSize", 512).toInt();
        
        int errorToCatch;
        
// -----------CALCULATE ARGUMENTS------------
        int argc;
        const char** argv = getRemoteInstanceArgv(settings, argc);
        
        compiledDSP = createRemoteDSPInstance(toCompile->fRemoteFactory, argc, argv, sampleRate, bufferSize, error_callback, error_callback_arg, errorToCatch);
        
        
        if(compiledDSP == NULL){
        
//----- If the factory is seen as already compiled but it disapeared, it has to be recompiled
            if(errorToCatch == ERROR_FACTORY_NOTFOUND){
                
                QPair<QString, void*> fS = createFactory(source, settings, errorMsg);
                
                if(fS.second == NULL){
                    errorMsg = "Impossible to find and recompile factory";
                    return NULL;
                }
                    
                compiledDSP = createDSP(fS, source, settings, error_callback, error_callback_arg, errorMsg);
            }
        }
        
        if(compiledDSP == NULL)
            errorMsg = getErrorFromCode(errorToCatch);
    }
#endif
    
    fDSPToFactory[compiledDSP] = mySetts;
    
    //-----Save settings
    if(compiledDSP != NULL && settings){
        settings->setValue("Path", path);
        settings->setValue("Name", name);
        settings->setValue("SHA", factorySetts.first);
	}
    
    return compiledDSP;
}

void FLSessionManager::deleteDSPandFactory(dsp* toDeleteDSP){
    
    factorySettings* factoryToDelete = fDSPToFactory[toDeleteDSP];
    fDSPToFactory.remove(toDeleteDSP);
    
    if(factoryToDelete->fType == TYPE_LOCAL){
        deleteDSPInstance((llvm_dsp*) toDeleteDSP);
        deleteDSPFactory(factoryToDelete->fFactory->fLLVMFactory);
    }
#ifdef REMOTE
    else{
        deleteRemoteDSPInstance((remote_dsp*) toDeleteDSP);
        deleteRemoteDSPFactory(factoryToDelete->fFactory->fRemoteFactory);
    }
#endif
}

//---------Session Management
//Saving the sources of the windows in their designated folders
void FLSessionManager::saveCurrentSources(const QString& sessionFolder){
    
    FLSettings* generalSettings = FLSettings::_Instance();
    
    generalSettings->beginGroup("Windows");
            
    QStringList groups = generalSettings->childGroups();
    
    for(int i=0; i<groups.size(); i++){
                
        QString settingsPath = groups[i] + "/SHA";
        QString shaValue = generalSettings->value(settingsPath, "").toString();
        
        QString shaPath = fSessionFolder + "/SHAFolder/" + shaValue + "/" + shaValue + ".dsp";
        QString savedPath = sessionFolder + "/Windows/FLW-" + groups[i] + "/" + shaValue + ".dsp";
        
        QFile shaSource(shaPath);
        shaSource.copy(savedPath);
    }
    generalSettings->endGroup();
        
}

//--Ask the user if he wants to save its DSP in an new location
QString FLSessionManager::askForSourceSaving(const QString& sourceContent){
    
    //------SLOTS FROM MENU ACTIONS THAT ARE REDIRECTED
    QMessageBox* existingNameMessage = new QMessageBox(QMessageBox::Warning, tr("Notification"), "Your DSP has no origin file.\n Do you want to save your code in a new file?");
    
    QPushButton* yes_Button = existingNameMessage->addButton(tr("Yes"), QMessageBox::AcceptRole);
    QPushButton* cancel_Button = existingNameMessage->addButton(tr("No"), QMessageBox::RejectRole);
    
    existingNameMessage->exec();
    if (existingNameMessage->clickedButton() == yes_Button){
        
        QFileDialog* fileDialog = new QFileDialog;
        fileDialog->setConfirmOverwrite(true);
        
        QString filename = fileDialog->getSaveFileName(NULL, "Save DSP", tr(""), tr("(*.dsp)"));
        
        if(QFileInfo(filename).suffix().indexOf("dsp") == -1)
            filename += ".dsp";
        
        writeFile(filename, sourceContent);
        
        return filename;
    }
    else
        return "";
}

//--Save temporary file from sha file
QString FLSessionManager::saveTempFile(const QString& shaSource){
    
    QString tempShaPath = fSessionFolder + "/Temp/temp_" + shaSource + ".dsp";

    QString shaPath = fSessionFolder + "/SHAFolder/" + shaSource + "/" + shaSource + ".dsp";
    
    QFile src(shaPath);    
    src.copy(tempShaPath);
    
    return tempShaPath;
    
}

//--Returns the content of sha file contained in the SHAFolder
QString FLSessionManager::contentOfShaSource(const QString& shaSource){
   
    QString shaPath = fSessionFolder + "/SHAFolder/" + shaSource + "/" + shaSource + ".dsp";
    
    return pathToContent(shaPath);
}

//--Restoration Menu
bool FLSessionManager::viewRestorationMsg(const QString& msg, const QString& yesMsg, const QString& noMsg){
    
    QMessageBox* existingNameMessage = new QMessageBox(QMessageBox::Warning, tr("Notification"), msg);
    QPushButton* yes_Button;
    
    existingNameMessage->setText(msg);
    yes_Button = existingNameMessage->addButton(yesMsg, QMessageBox::AcceptRole);
    existingNameMessage->addButton(noMsg, QMessageBox::RejectRole);

    existingNameMessage->exec();
    
    return (existingNameMessage->clickedButton() == yes_Button);
}

//Behaviour of session restoration when re-starting the application
//The user is notified in case of source file lost or modified. He can choose to reload from original file or backup.
map<int, QString> FLSessionManager::currentSessionRestoration(){

    map<int, QString> windowIndexToSource;
    
//If 2 windows are pointing on the same lost source, the Dialog has not to appear twice
    QMap<QString, int> updated;
    
    FLSettings* generalSettings = FLSettings::_Instance();
    
    generalSettings->beginGroup("Windows");
    
    QStringList groups = generalSettings->childGroups();
    for(int i=0; i<groups.size(); i++){
               
        QString shaPath = groups[i] + "/SHA";
        QString shaValue = generalSettings->value(shaPath, "").toString();
        
        QString recallingPath = fSessionFolder + "/Windows/FLW-" + groups[i] + "/" + shaValue + ".dsp";
        QString savedContent = pathToContent(recallingPath);
        
        QString settingsPath = groups[i] + "/Path";
        QString originalPath = generalSettings->value(settingsPath, "").toString();
        
//        In Case DSP Source Is Not a DSP File
        if(originalPath == "")
            windowIndexToSource[groups[i].toInt()] = savedContent;
//        In case it is a DSP
        else{
//              In case path restoration was already treated
            if(updated.contains(originalPath)){
                windowIndexToSource[groups[i].toInt()] = windowIndexToSource[updated[originalPath]];
            }
            else{

                QString originalContent = pathToContent(originalPath);
                
                //            In Case The Original File Was Deleted
                if(!QFileInfo(originalPath).exists()){
                    QString msg = originalPath + " cannot be found! Do you want to reload it from an internal copy of your file?";
                    if(viewRestorationMsg(msg, "Yes", "No"))
                        windowIndexToSource[groups[i].toInt()] = savedContent;
                    else
                        windowIndexToSource[groups[i].toInt()] = "";
                }
                //            In Case The Original Content is Modified
                else if(QFileInfo(recallingPath).exists() && savedContent != originalContent){
                    
                    QString msg = "The content of " + originalPath + " was modified. Do you want to reload your original file or the internal copy?";
                    
                    if (viewRestorationMsg(msg, "Copied File", "Original File"))
                        windowIndexToSource[groups[i].toInt()] = savedContent; 
                    else
                        windowIndexToSource[groups[i].toInt()] = originalPath;
                }
                //             In Normal Case
                else
                    windowIndexToSource[groups[i].toInt()] = originalPath;
                
//                Declaration as treated
                updated[originalPath] = groups[i].toInt();
            }
        }
            
    }
    
    generalSettings->endGroup();

    return windowIndexToSource;
}

void FLSessionManager::copySHAFolder(const QString& snapshotFolder){
    
    QString shaFolder = fSessionFolder + "/SHAFolder";
    QString shaSnapshotFolder = snapshotFolder + "/SHAFolder";
    
    QDir snapshotDir(shaSnapshotFolder);
    
    QFileInfoList children = snapshotDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    QFileInfoList::iterator it;
    
    for(it = children.begin(); it != children.end(); it++){
        
        QString destinationFolder = shaFolder + "/" +  it->baseName();
        cpDir(it->absoluteFilePath(), destinationFolder);
    }
}

map<int, QString>   FLSessionManager::snapshotRestoration(const QString& file){
    
    QString filename = file;
    
    if(QFileInfo(filename).completeSuffix() != "tar")
        filename += ".tar";
    
#ifndef _WIN32
    QString error;
    if(!untarFolder(filename, error))
        FLErrorWindow::_Instance()->print_Error(error);
#endif
    
    QString snapshotFolder = QFileInfo(filename).canonicalPath() + "/" + QFileInfo(filename).baseName();
    
    copySHAFolder(snapshotFolder);
    
    map<int, QString> windowIndexToSource;
    
    //If 2 windows are pointing on the same lost source, the Dialog has not to appear twice
    QMap<QString, int> updated;
    
    QString snapshotSettings = snapshotFolder + "/Settings.ini";
    
    QSettings* generalSettings = new QSettings(snapshotSettings, QSettings::IniFormat);
    
    generalSettings->beginGroup("Windows");
    
    QStringList groups = generalSettings->childGroups();
    
    for(int i=0; i<groups.size(); i++){
        
        QString shaPath = groups[i] + "/SHA";
        QString shaValue = generalSettings->value(shaPath, "").toString();
        
        QString recallingPath = snapshotFolder + "/Windows/FLW-" + groups[i] + "/" + shaValue + ".dsp";
        QString savedContent = pathToContent(recallingPath);
        
        QString settingsPath = groups[i] + "/Path";
        QString originalPath = generalSettings->value(settingsPath, "").toString();
        
        //        In Case DSP Source Is Not a DSP File
        if(originalPath == "")
            windowIndexToSource[groups[i].toInt()] = savedContent;
        //        In case it is a DSP
        else{
            //              In case path restoration was already treated
            if(updated.contains(originalPath))
                windowIndexToSource[groups[i].toInt()] = windowIndexToSource[updated[originalPath]];
            else{
        
                QString originalContent = pathToContent(originalPath);
    
                //            In Case The Original File Was Deleted
                if(!QFileInfo(originalPath).exists() || savedContent != originalContent){
                    windowIndexToSource[groups[i].toInt()] = savedContent;
                //   ATTENTION : ICI IL FAUT ENVOYER UN MESSAGE POUR PREVENIR QU'ON TRAVAILLE SUR UNE COPIE
                    // ET IL FAUT FAIRE UN TRUC POUR PAS QU'ON TE LE RAPPELLE À CHAQUE RAPPEL DE CETTE SESSION 
                }
                else{
                    windowIndexToSource[groups[i].toInt()] = originalPath;
                }
                updated[originalPath] = groups[i].toInt();
            }
        }
    }
    
    generalSettings->endGroup();
    
    return windowIndexToSource;
}

//    Pour l'histoire du contenu modifié ou du path, il faut continuer à vérifier le fichier original par rapport au fichier SHA. 
//    --> Un problème relou d'aujourd'hui à résoudre est si : tu as rappelé un snapshot, ses fichiers originaux ont été modifié. Tu sais déjà qu'on travaille sur des copies mais on te le rappelle à chaque fois que tu restaures cette session. Le mieux serait de le prendre en compte dans le contenu du snapshot.
void FLSessionManager::createSnapshot(const QString& snapshotFolder){
    
    QDir snapshot(snapshotFolder);
    snapshot.mkdir(snapshotFolder);
    
    QString shaFolder = fSessionFolder + "/SHAFolder";
    QString shaSnapshotFolder = snapshotFolder + "/SHAFolder";
    
    QDir shaDir(shaSnapshotFolder);
    shaDir.mkdir(shaSnapshotFolder);
    
    QSettings* generalSettings = FLSettings::_Instance();
    
    generalSettings->beginGroup("Windows");
    
    QStringList groups = generalSettings->childGroups();
    
    for(int i=0; i<groups.size(); i++){
        
        QString shaCS = groups[i] + "/SHA";
        QString shaSF = generalSettings->value(shaCS, "").toString();

        QString srcFolder = shaFolder + "/" + shaSF;
        QString dstFolder = shaSnapshotFolder + "/" + shaSF;
        
        QDir dstDir(dstFolder);
        dstDir.mkdir(dstFolder);
        
        cpDir(srcFolder, dstFolder);
    }
    
    generalSettings->endGroup();
    
    QString winFolder = fSessionFolder + "/Windows";
    QString winSnapshotFolder = snapshotFolder + "/Windows";
    
    cpDir(winFolder, winSnapshotFolder);
    
    QString settingsFile = fSessionFolder + "/Settings.ini";
    QString settingsSnapshotFile = snapshotFolder + "/Settings.ini";
    
    QFile f(settingsFile);
    f.copy(settingsSnapshotFile);
    
    saveCurrentSources(snapshotFolder);

#ifndef _WIN32
    QString error("");
    if(tarFolder(snapshotFolder, error))
        deleteDirectoryAndContent(snapshotFolder);
    else
        FLErrorWindow::_Instance()->print_Error(error);
#endif
}
          
//Calculate the faust expanded version
QString FLSessionManager::get_expandedVersion(QSettings* settings, const QString& source){
    
    string name_app = settings->value("Name", "").toString().toStdString();
    string sha_key = settings->value("SHA", "").toString().toStdString();
    
    string dsp_content = source.toStdString();
    
    if(QFileInfo(source).exists())
        dsp_content = pathToContent(source).toStdString();
    
    int argc = 0;
    
    QString defaultOptions = FLSettings::_Instance()->value("General/Compilation/FaustOptions", "").toString();
    
    QString faustOptions = settings->value("Compilation/FaustOptions", defaultOptions).toString();
    
    const char** argv = getFactoryArgv("", "", faustOptions, argc);
    
    string error_msg("");
    
    return QString(expandDSPFromString(name_app, dsp_content, argc, argv, sha_key, error_msg).c_str());
}

QVector<QString> FLSessionManager::get_dependencies(dsp* myDSP, const QString& path){
    
    factorySettings* setts = fDSPToFactory[myDSP];
    
    factory* factoryDependency = setts->fFactory;
    
    QVector<QString> dependencies;
    std::vector<std::string> stdDependendies;
    
    if(setts->fType == TYPE_LOCAL)
       stdDependendies = getLibraryList(factoryDependency->fLLVMFactory);
//    else
//        stdDependendies = factoryDependency->fRemoteFactory->getLibraryList();
    
    for(size_t i = 0; i<stdDependendies.size(); i++){
        dependencies.push_back(stdDependendies[i].c_str());
    }
    
    if(path != "")
        dependencies.push_back(path);
    
    return dependencies;
}









