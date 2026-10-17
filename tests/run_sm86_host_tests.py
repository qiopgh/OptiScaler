"""运行已准备的独立宿主目录，验证实际功能绑定、配置保留与后端隔离。"""
import argparse
import hashlib
import json
import shutil
import subprocess
from pathlib import Path


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('fixtures', type=Path, help='包含 native、sm86、missing、conflict 的独立目录')
    parser.add_argument('--binary', type=Path, required=True, help='待验证的 OptiScaler DLL')
    parser.add_argument('--probe', type=Path, required=True, help='已构建的 sm86_host_probe.exe')
    parser.add_argument('--expected-preset', help='可选：核对当前诊断配置指定的模型，不影响运行时配置')
    args = parser.parse_args()
    root = args.fixtures.resolve()
    results = {'binary_sha256': digest(args.binary), 'runtime_acceptance': '待用户游戏内验证', 'cases': {}}
    for name in ['native', 'sm86', 'missing', 'conflict']:
        target = root / name
        # 只更新本脚本已准备的诊断副本，不能把游戏目录传入此参数。
        if not (target / 'OptiScaler.ini').is_file() or not (target / 'sm86_host_probe.exe').is_file():
            raise RuntimeError('诊断目录不完整：' + str(target))
        shutil.copy2(args.binary, target / 'dxgi.dll')
        shutil.copy2(args.probe, target / 'sm86_host_probe.exe')
        upstream_ini = target / 'OptiScaler/dlssg_sm86_proxy/dlssg_sm86.ini'
        original_config = upstream_ini.read_bytes() if upstream_ini.is_file() else None
        prior_logs = set((target / 'OptiScaler/dlssg_sm86_proxy/dlssg_sm86/logs').glob('*.jsonl'))
        command = [str(target / 'sm86_host_probe.exe'), str(target / 'dxgi.dll'), name]
        if name == 'conflict':
            command.append(str(target / 'OptiScaler/streamline/nvngx_dlssg.dll'))
        run = subprocess.run(command, cwd=target, capture_output=True, timeout=45)
        output = (run.stdout + run.stderr).decode('utf-8', errors='replace')
        (root / (name + '-final-run.log')).write_text(output, encoding='utf-8')
        log = (target / 'OptiScaler.log').read_text(encoding='utf-8-sig')
        (root / (name + '-final-OptiScaler.log')).write_text(log, encoding='utf-8')
        checks = {'host': run.returncode == 0 and 'BACKEND_ISOLATION=PASS' in output}
        checks['upstream_config_unchanged'] = (upstream_ini.read_bytes() if upstream_ini.is_file() else None) == original_config
        current_logs = set((target / 'OptiScaler/dlssg_sm86_proxy/dlssg_sm86/logs').glob('*.jsonl'))
        if name == 'sm86':
            checks['active_feature_binding'] = 'Active Streamline feature binding: true' in log
            checks['actual_device_support'] = all(marker in output for marker in [
                'DLSSG_BINDING=0', 'DLSSG_STATE_QUERY=0', 'DLSSG_DEVICE_SUPPORT=0'])
            rows = [json.loads(line) for path in current_logs - prior_logs
                    for line in path.read_text(encoding='utf-8-sig').splitlines() if line.strip()]
            # 上游日志是当前诊断的辅助观察，格式和模型选择均不作为通用版本门槛。
            results['upstream_observations'] = {
                'backend_install_seen': any(row.get('event') == 'backend_install' and row.get('status') == 0 for row in rows),
                'presets_seen': sorted({str(row['preset']) for row in rows if 'preset' in row}),
                'preset_pinned_seen': any(row.get('preset_pinned') is True for row in rows)}
            if args.expected_preset:
                checks['requested_diagnostic_preset'] = any(str(row.get('preset', '')).upper() == args.expected_preset.upper()
                                                            for row in rows)
            checks['no_initialization_error'] = 'Streamline 初始化失败' not in log
        elif name == 'missing':
            checks['missing_component_rejected'] = 'SM75/SM86 组件缺失' in log
        elif name == 'conflict':
            checks['other_runtime_rejected'] = 'SM75/SM86 后端拒绝混用' in log
        results['cases'][name] = checks
        print(name + ': ' + ', '.join(key + '=' + ('PASS' if value else 'FAIL') for key, value in checks.items()))
    (root / 'general-final-results.json').write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding='utf-8')
    if not all(value for checks in results['cases'].values() for value in checks.values()):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
