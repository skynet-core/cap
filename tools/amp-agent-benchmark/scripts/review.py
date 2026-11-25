#!/usr/bin/env python3
"""Apply independent human review decisions and recompute accepted-task throughput."""
import argparse,json,pathlib
p=argparse.ArgumentParser();p.add_argument('run_dir');p.add_argument('--approvals',required=True,help='JSON map task-id -> {approved: bool, reviewer: str, note: str}')
a=p.parse_args();root=pathlib.Path(a.run_dir)
records=json.loads((root/'tasks.json').read_text());decisions=json.loads(pathlib.Path(a.approvals).read_text())
for record in records:
    d=decisions.get(record['id'])
    if d is None:continue
    if not isinstance(d,dict) or not d.get('reviewer') or not isinstance(d.get('approved'),bool):raise ValueError('review requires approved bool and reviewer')
    record['review']='approved' if d['approved'] else 'rejected'
    record['reviewer']=d['reviewer'];record['review_note']=d.get('note','')
    record['accepted']=bool(record.get('candidate_passed')) and d['approved']
(root/'tasks.json').write_text(json.dumps(records,indent=2))
summary=json.loads((root/'summary.json').read_text());summary['accepted_tasks']=sum(bool(x.get('accepted')) for x in records)
summary['accepted_tasks_per_hour']=round(summary['accepted_tasks']*3600/summary['elapsed_s'],3) if summary['elapsed_s'] else None
(root/'summary.json').write_text(json.dumps(summary,indent=2))
print(json.dumps({'accepted_tasks':summary['accepted_tasks'],'accepted_tasks_per_hour':summary['accepted_tasks_per_hour']},indent=2))
