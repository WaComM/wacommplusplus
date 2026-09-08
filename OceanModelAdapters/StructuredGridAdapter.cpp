#include "StructuredGridAdapter.hpp"
#include "../NumericalHelpers.hpp"

#include <algorithm>
#include <stdexcept>

StructuredGridAdapter::StructuredGridAdapter(string &fileName, string model): fileName(fileName), model(model) {
    logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("WaComM"));
}

StructuredGridAdapter::~StructuredGridAdapter() = default;

NcVar StructuredGridAdapter::variable(NcFile &dataFile, const vector<string> &names, bool optional) {
    for (const string &name:names) {
        NcVar value=dataFile.getVar(name);
        if (!value.isNull()) return value;
    }
    if (optional) return NcVar();
    string aliases;
    for (const string &name:names) aliases+=(aliases.empty() ? "" : ", ")+name;
    throw std::runtime_error(model + " input is missing required variable (aliases: " + aliases + ")");
}

void StructuredGridAdapter::process() {
    LOG4CPLUS_DEBUG(logger,model << " file loading:" << fileName);
    NcFile dataFile(fileName,NcFile::read);

    NcVar timeVar=variable(dataFile,model == "NEMO" ? vector<string>{"time_counter","time"} : vector<string>{"time","MT"});
    NcVar lonVar=variable(dataFile,model == "NEMO" ? vector<string>{"nav_lon","longitude","lon"} : vector<string>{"lon","longitude"});
    NcVar latVar=variable(dataFile,model == "NEMO" ? vector<string>{"nav_lat","latitude","lat"} : vector<string>{"lat","latitude"});
    NcVar depthVar=variable(dataFile,model == "NEMO" ? vector<string>{"deptht","depth"} : vector<string>{"depth"});
    NcVar uVar=variable(dataFile,model == "NEMO" ? vector<string>{"uo","vozocrtx","u"} : vector<string>{"water_u","u"});
    NcVar vVar=variable(dataFile,model == "NEMO" ? vector<string>{"vo","vomecrty","v"} : vector<string>{"water_v","v"});

    vector<NcDim> uDims=uVar.getDims();
    vector<NcDim> vDims=vVar.getDims();
    if (uDims.size()!=4 || vDims.size()!=4 || uDims[2].getSize()!=vDims[2].getSize() || uDims[3].getSize()!=vDims[3].getSize()) {
        throw std::runtime_error(model + " adapter requires common horizontal U/V dimensions; staggered grids are unsupported");
    }

    size_t ocean_time=uDims[0].getSize(),s_rho=uDims[1].getSize(),s_w=s_rho+1;
    size_t eta_rho=uDims[2].getSize(),xi_rho=uDims[3].getSize();
    this->OceanTime().Allocate(ocean_time);
    this->SRho().Allocate(s_rho,-(int)s_rho+1);
    this->SW().Allocate(s_w,-(int)s_w+1);
    this->DepthIntervals().Allocate(s_w,-(int)s_w+2);
    this->Mask().Allocate(eta_rho,xi_rho);
    this->Lon().Allocate(eta_rho,xi_rho); this->Lat().Allocate(eta_rho,xi_rho);
    this->LonRad().Allocate(eta_rho,xi_rho); this->LatRad().Allocate(eta_rho,xi_rho);
    this->H().Allocate(eta_rho,xi_rho); this->Zeta().Allocate(ocean_time,eta_rho,xi_rho);
    this->U().Allocate(ocean_time,s_rho,eta_rho,xi_rho,0,-(int)s_rho+1,0,0);
    this->V().Allocate(ocean_time,s_rho,eta_rho,xi_rho,0,-(int)s_rho+1,0,0);
    this->W().Allocate(ocean_time,s_w,eta_rho,xi_rho,0,-(int)s_w+1,0,0);
    this->AKT().Allocate(ocean_time,s_w,eta_rho,xi_rho,0,-(int)s_w+1,0,0);
    timeVar.getVar(this->OceanTime()()); uVar.getVar(this->U()()); vVar.getVar(this->V()());

    vector<double> depth(s_rho); depthVar.getVar(depth.data());
    double maximumDepth=*std::max_element(depth.begin(),depth.end());
    for (int k=-(int)s_rho+1;k<=0;k++) this->SRho()(k)=-depth[NumericalHelpers::storageLevel(k,s_rho)]/maximumDepth;
    for (int k=-(int)s_w+1;k<=0;k++) this->SW()(k)=(double)k/(s_w-1);
    for (int k=-(int)s_w+2;k<=0;k++) this->DepthIntervals()(k)=this->SW()(k)-this->SW()(k-1);

    vector<NcDim> lonDims=lonVar.getDims(),latDims=latVar.getDims();
    if (lonDims.size()==2 && latDims.size()==2) { lonVar.getVar(this->Lon()()); latVar.getVar(this->Lat()()); }
    else if (lonDims.size()==1 && latDims.size()==1 && lonDims[0].getSize()==xi_rho && latDims[0].getSize()==eta_rho) {
        vector<double> longitude(xi_rho),latitude(eta_rho); lonVar.getVar(longitude.data()); latVar.getVar(latitude.data());
        for (int j=0;j<eta_rho;j++) for (int i=0;i<xi_rho;i++) { this->Lon()(j,i)=longitude[i]; this->Lat()(j,i)=latitude[j]; }
    } else throw std::runtime_error(model + " longitude/latitude dimensions are incompatible with velocity fields");

    NcVar maskVar=variable(dataFile,model == "NEMO" ? vector<string>{"tmask","mask"} : vector<string>{"mask"},true);
    NcVar hVar=variable(dataFile,model == "NEMO" ? vector<string>{"bathymetry","h"} : vector<string>{"bathymetry"},true);
    NcVar zetaVar=variable(dataFile,model == "NEMO" ? vector<string>{"zos","sossheig","ssh"} : vector<string>{"surf_el","ssh","zeta"},true);
    NcVar wVar=variable(dataFile,model == "NEMO" ? vector<string>{"wo","vovecrtz","w"} : vector<string>{"water_w","w"},true);
    NcVar aktVar=variable(dataFile,model == "NEMO" ? vector<string>{"avt","votkeavt","akt"} : vector<string>{"diffusivity","akt"},true);

    if (!maskVar.isNull() && maskVar.getDimCount()==2) maskVar.getVar(this->Mask()());
    else for (int j=0;j<eta_rho;j++) for (int i=0;i<xi_rho;i++) this->Mask()(j,i)=1;
    if (!hVar.isNull()) hVar.getVar(this->H()());
    else for (int j=0;j<eta_rho;j++) for (int i=0;i<xi_rho;i++) this->H()(j,i)=maximumDepth;
    if (!zetaVar.isNull()) zetaVar.getVar(this->Zeta()());
    else for (int t=0;t<ocean_time;t++) for (int j=0;j<eta_rho;j++) for (int i=0;i<xi_rho;i++) this->Zeta()(t,j,i)=0;
    if (!wVar.isNull() && wVar.getDimCount()==4 && wVar.getDim(1).getSize()==s_w) wVar.getVar(this->W()());
    else { LOG4CPLUS_WARN(logger,model << " vertical velocity is missing; using W=0"); for (int t=0;t<ocean_time;t++) for (int k=-(int)s_w+1;k<=0;k++) for (int j=0;j<eta_rho;j++) for (int i=0;i<xi_rho;i++) this->W()(t,k,j,i)=0; }
    if (!aktVar.isNull() && aktVar.getDimCount()==4 && aktVar.getDim(1).getSize()==s_w) aktVar.getVar(this->AKT()());
    else { LOG4CPLUS_WARN(logger,model << " diffusivity is missing; using AKT=0"); for (int t=0;t<ocean_time;t++) for (int k=-(int)s_w+1;k<=0;k++) for (int j=0;j<eta_rho;j++) for (int i=0;i<xi_rho;i++) this->AKT()(t,k,j,i)=0; }

    for (int j=0;j<eta_rho;j++) for (int i=0;i<xi_rho;i++) {
        if (model == "HYCOM" && this->Lon()(j,i)>180) this->Lon()(j,i)-=360;
        this->LonRad()(j,i)=0.017453292519943295*this->Lon()(j,i);
        this->LatRad()(j,i)=0.017453292519943295*this->Lat()(j,i);
    }
}
