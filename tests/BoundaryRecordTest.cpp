#include "../OceanModelAdapter.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
class FixtureAdapter: public OceanModelAdapter {
public:
    void process() override {}
};

// Distinct bit patterns per field, record, and cell, including negative zero
float value(int field, int t, int k, int j, int i, double start) {
    if (field==1 && t==0 && k==0 && j==0 && i==0) return -0.0f;
    return (float)(field*1000.0+start/60.0+t*100.0+k*10.0+j+i*0.01+1.0/3.0);
}

// A window of nt records on a non-square grid with fields that differ in every element
void fill(FixtureAdapter &adapter, int nt, double start, int sRho, int sW, int eta, int xi) {
    adapter.OceanTime().Allocate(nt);
    for (int t=0;t<nt;t++) adapter.OceanTime()(t)=start+3600.0*t;
    adapter.SRho().Allocate(sRho,-sRho+1);
    for (int k=-sRho+1;k<=0;k++) adapter.SRho()(k)=(k-0.5)/sRho;
    adapter.SW().Allocate(sW,-sW+1);
    for (int k=-sW+1;k<=0;k++) adapter.SW()(k)=(double)k/(sW-1);
    adapter.Mask().Allocate(eta,xi); adapter.Lon().Allocate(eta,xi); adapter.Lat().Allocate(eta,xi); adapter.H().Allocate(eta,xi);
    for (int j=0;j<eta;j++) for (int i=0;i<xi;i++) {
        adapter.Mask()(j,i)=1; adapter.Lon()(j,i)=14+0.1*i; adapter.Lat()(j,i)=40+0.1*j; adapter.H()(j,i)=50+j+i;
    }
    adapter.Zeta().Allocate(nt,eta,xi);
    adapter.U().Allocate(nt,sRho,eta,xi,0,-sRho+1,0,0); adapter.V().Allocate(nt,sRho,eta,xi,0,-sRho+1,0,0);
    adapter.W().Allocate(nt,sW,eta,xi,0,-sW+1,0,0); adapter.AKT().Allocate(nt,sW,eta,xi,0,-sW+1,0,0);
    for (int t=0;t<nt;t++) for (int j=0;j<eta;j++) for (int i=0;i<xi;i++) {
        adapter.Zeta()(t,j,i)=value(0,t,0,j,i,start);
        for (int k=-sRho+1;k<=0;k++) { adapter.U()(t,k,j,i)=value(1,t,k,j,i,start); adapter.V()(t,k,j,i)=value(2,t,k,j,i,start); }
        for (int k=-sW+1;k<=0;k++) { adapter.W()(t,k,j,i)=value(3,t,k,j,i,start); adapter.AKT()(t,k,j,i)=value(4,t,k,j,i,start); }
    }
}

bool same(float a, float b) { return std::memcmp(&a,&b,sizeof(float))==0; }

// Element-wise reference: the window's records with the adjacent record before or after them
void assertExtended(OceanModelAdapter &extended, FixtureAdapter &original, FixtureAdapter &adjacent, int record, bool prepend,
                    int sRho, int sW, int eta, int xi) {
    int oldTime=(int)original.OceanTime().Nx();
    assert((int)extended.OceanTime().Nx()==oldTime+1);
    for (int t=0;t<=oldTime;t++) {
        bool boundary=prepend ? t==0 : t==oldTime;
        FixtureAdapter &source=boundary ? adjacent : original;
        int s=boundary ? record : (prepend ? t-1 : t);
        assert(extended.OceanTime()(t)==source.OceanTime()(s));
        for (int j=0;j<eta;j++) for (int i=0;i<xi;i++) {
            assert(same(extended.Zeta()(t,j,i),source.Zeta()(s,j,i)));
            for (int k=-sRho+1;k<=0;k++) {
                assert(same(extended.U()(t,k,j,i),source.U()(s,k,j,i)));
                assert(same(extended.V()(t,k,j,i),source.V()(s,k,j,i)));
            }
            for (int k=-sW+1;k<=0;k++) {
                assert(same(extended.W()(t,k,j,i),source.W()(s,k,j,i)));
                assert(same(extended.AKT()(t,k,j,i),source.AKT()(s,k,j,i)));
            }
        }
    }
}

// Copy one record of every field from a window into another, so that both hold the same boundary
void copyRecord(OceanModelAdapter &target, int to, OceanModelAdapter &source, int from, int sRho, int sW, int eta, int xi) {
    target.OceanTime()(to)=source.OceanTime()(from);
    for (int j=0;j<eta;j++) for (int i=0;i<xi;i++) {
        target.Zeta()(to,j,i)=source.Zeta()(from,j,i);
        for (int k=-sRho+1;k<=0;k++) { target.U()(to,k,j,i)=source.U()(from,k,j,i); target.V()(to,k,j,i)=source.V()(from,k,j,i); }
        for (int k=-sW+1;k<=0;k++) { target.W()(to,k,j,i)=source.W()(from,k,j,i); target.AKT()(to,k,j,i)=source.AKT()(from,k,j,i); }
    }
}

// The window still holds exactly its original records
void assertUnchanged(OceanModelAdapter &window, FixtureAdapter &original, int sRho, int sW, int eta, int xi) {
    assert(window.OceanTime().Nx()==original.OceanTime().Nx());
    for (int t=0;t<(int)original.OceanTime().Nx();t++) for (int j=0;j<eta;j++) for (int i=0;i<xi;i++) {
        assert(window.OceanTime()(t)==original.OceanTime()(t) && same(window.Zeta()(t,j,i),original.Zeta()(t,j,i)));
        for (int k=-sRho+1;k<=0;k++) assert(same(window.U()(t,k,j,i),original.U()(t,k,j,i)) && same(window.V()(t,k,j,i),original.V()(t,k,j,i)));
        for (int k=-sW+1;k<=0;k++) assert(same(window.W()(t,k,j,i),original.W()(t,k,j,i)) && same(window.AKT()(t,k,j,i),original.AKT()(t,k,j,i)));
    }
}
}

int main() {
    const int sRho=3, sW=4, eta=5, xi=7;

    // Forward: the adjacent window's first record follows the last one
    {
        FixtureAdapter window, original, next;
        fill(window,3,0,sRho,sW,eta,xi); fill(original,3,0,sRho,sW,eta,xi); fill(next,2,3*3600.0,sRho,sW,eta,xi);
        window.appendBoundaryRecord(next,0,false);
        assertExtended(window,original,next,0,false,sRho,sW,eta,xi);

        // A second window appended to the extended one keeps every record
        FixtureAdapter extended, after;
        fill(extended,3,0,sRho,sW,eta,xi); extended.appendBoundaryRecord(next,0,false);
        fill(after,2,4*3600.0,sRho,sW,eta,xi);
        window.appendBoundaryRecord(after,0,false);
        assertExtended(window,extended,after,0,false,sRho,sW,eta,xi);
    }

    // Backward: the previous window's last record precedes the first one
    {
        FixtureAdapter window, original, previous;
        fill(window,3,2*3600.0,sRho,sW,eta,xi); fill(original,3,2*3600.0,sRho,sW,eta,xi); fill(previous,2,0,sRho,sW,eta,xi);
        window.appendBoundaryRecord(previous,1,true);
        assertExtended(window,original,previous,1,true,sRho,sW,eta,xi);
    }

    // An adjacent window whose fields do not match its own dimensions is rejected; the window is unchanged
    {
        FixtureAdapter window, original, next;
        fill(window,3,0,sRho,sW,eta,xi); fill(original,3,0,sRho,sW,eta,xi); fill(next,2,3*3600.0,sRho,sW,eta,xi);
        next.U().Allocate(1,sRho,eta,xi,0,-sRho+1,0,0);
        bool rejected=false;
        try { window.appendBoundaryRecord(next,0,false); }
        catch (const std::runtime_error &) { rejected=true; }
        assert(rejected && window.OceanTime().Nx()==3);
        assert(same(window.U()(2,0,eta-1,xi-1),original.U()(2,0,eta-1,xi-1)));
    }

    // Saved windows that already hold the adjacent boundary keep their records, forward and backward;
    // the values are compared numerically, so a negative zero matches a positive zero
    for (bool prepend: {false,true}) {
        FixtureAdapter window, original, adjacent;
        fill(window,3,3600.0,sRho,sW,eta,xi); fill(adjacent,2,prepend ? 0.0 : 3*3600.0,sRho,sW,eta,xi);
        int endpoint=prepend ? 0 : 2, record=prepend ? 1 : 0;
        window.U()(endpoint,0,0,0)=-0.0f;
        copyRecord(adjacent,record,window,endpoint,sRho,sW,eta,xi);
        adjacent.U()(record,0,0,0)=0.0f;
        fill(original,3,3600.0,sRho,sW,eta,xi); original.U()(endpoint,0,0,0)=-0.0f;
        window.appendBoundaryRecord(adjacent,record,prepend);
        assertUnchanged(window,original,sRho,sW,eta,xi);

        // A conflict in the last stored value of any field is rejected and leaves the window unchanged
        float *last[]={&adjacent.Zeta()(record,eta-1,xi-1),&adjacent.U()(record,0,eta-1,xi-1),&adjacent.V()(record,0,eta-1,xi-1),
                       &adjacent.W()(record,0,eta-1,xi-1),&adjacent.AKT()(record,0,eta-1,xi-1)};
        const char *messages[]={"surface elevation","horizontal velocity","horizontal velocity","vertical fields","vertical fields"};
        for (int f=0;f<5;f++) {
            float value=*last[f]; *last[f]=value+1.0f;
            bool rejected=false;
            try { window.appendBoundaryRecord(adjacent,record,prepend); }
            catch (const std::runtime_error &e) { rejected=std::string(e.what()).find(messages[f])!=std::string::npos; }
            assert(rejected);
            *last[f]=value;
        }
        assertUnchanged(window,original,sRho,sW,eta,xi);
    }
}
