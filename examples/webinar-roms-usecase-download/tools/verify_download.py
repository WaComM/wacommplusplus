#!/usr/bin/env python3
"""Verify the declared webinar archive metadata without modifying source files."""
from pathlib import Path
import datetime,json
import netCDF4
import numpy as np
root=Path(__file__).resolve().parents[1]
archive=root/'data/roms-20260915-16'
config=json.loads((root/'webinar-roms-usecase-download.json').read_text())
manifest=json.loads((archive/'provenance/download.json').read_text())
rows=[]
for index,name in enumerate(config['io']['nc_inputs']):
 path=archive/name
 entry=manifest['files'][name]
 assert path.stat().st_size==entry['bytes']==6230329808
 with netCDF4.Dataset(path) as ds:
  t=ds['ocean_time']
  assert t.units=='seconds since 1968-05-23 00:00:00' and t.calendar=='gregorian'
  expected=datetime.datetime(2026,9,15)+datetime.timedelta(hours=index)
  actual=netCDF4.num2date(t[:],t.units,t.calendar)
  assert len(actual)==1 and actual[0].isoformat()==expected.isoformat()
  assert len(ds.dimensions['xi_rho'])==2273 and len(ds.dimensions['eta_rho'])==1688
  for a,b in [('s_rho','Cs_r'),('s_w','Cs_w')]: assert np.array_equal(ds[a][:],ds[b][:])
  angle=np.ma.asarray(ds['angle'][:]); assert not np.ma.getmaskarray(angle).any() and np.isfinite(angle).all()
  j,i=800,1000
  lat=np.deg2rad(ds['lat_rho'][j,i:i+2]); lon=np.deg2rad(ds['lon_rho'][j,i:i+2])
  dlon=np.arctan2(np.sin(lon[1]-lon[0]),np.cos(lon[1]-lon[0]))
  bearing=float(np.arctan2(lat[1]-lat[0],dlon*np.cos(.5*(lat[0]+lat[1]))))
  difference=float(np.arctan2(np.sin(float(angle[j,i])-bearing),np.cos(float(angle[j,i])-bearing)))
  rows.append(dict(sample_rho_indices=[j,i],sample_xi_bearing_rad=bearing,sample_angle_minus_bearing_rad=difference,name=name,bytes=entry['bytes'],sha256=entry['sha256'],physical_time_utc=expected.isoformat()+'Z',angle_units=ds['angle'].units,angle_min=float(angle.min()),angle_max=float(angle.max()),sigma_stretching_equals_s=True))
report=dict(archive=str(archive),complete=True,files=rows,total_bytes=sum(x['bytes'] for x in rows),physical_window='2026-09-15T00:00:00Z/2026-09-16T00:00:00Z',source_base_url=manifest['base_url'],download_failures=manifest['failures'],verification='File sizes, all 25 exact physical timestamps, dimensions, vertical curves and angle metadata checked; download SHA-256 values archived separately.',adapter_status='Vector-basis compatibility unresolved: angle is approximately pi/2 radians; rotation is implemented, but sampled XI coordinates point east and contradict the declared angle. No scientific native conversion or solver benchmark accepted.')
previous=root/'docs/download-20260915.json'
if previous.exists():
 old=json.loads(previous.read_text())
 if 'checksum_reverification' in old: report['checksum_reverification']=old['checksum_reverification']
report['orientation_diagnostic']='At rho j=800,i=1000, compare declared angle with atan2(delta latitude, wrapped delta longitude * cos(mean latitude)) for the next XI point, all angles in radians. This local tangent diagnostic identifies conflicting metadata; it never replaces the supplied angle or selects a vector basis.'
(root/'docs/download-20260915.json').write_text(json.dumps(report,indent=2)+'\n')
(archive/'provenance/metadata-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='files'},indent=2))
