import React, { useMemo, useState } from 'react';
import type { FileInfoDetails } from '../../types';

interface FileInfoPanelProps {
  details: FileInfoDetails | null;
  onReload: () => void;
  onSave: (payload: { description: string; keywords: string; category: string }) => void;
}

export default function FileInfoPanel({ details, onReload, onSave }: FileInfoPanelProps) {
  const [editing, setEditing] = useState(false);
  const [description, setDescription] = useState('');
  const [keywords, setKeywords] = useState('');
  const [category, setCategory] = useState('');

  const seedFromDetails = useMemo(() => {
    if (!details) return '';
    return JSON.stringify([details.id, details.description, details.keywords, details.category]);
  }, [details]);

  React.useEffect(() => {
    if (!details) return;
    setDescription(details.description || '');
    setKeywords(details.keywords || '');
    setCategory(details.category || '');
    setEditing(false);
  }, [seedFromDetails, details]);

  if (!details) {
    return <div className="panel-placeholder">请选择一个文件查看详细信息</div>;
  }

  return (
    <div className="file-info-panel">
      <div className="cover-card">
        <div className="cover-img-placeholder">{details.hasCover ? 'HAS COVER' : 'NO COVER'}</div>
        <div className="cover-name">{details.fileName}</div>
      </div>

      <div className="file-info-actions">
        <button className="btn-secondary btn-compact" onClick={onReload}>刷新</button>
        {!editing ? (
          <button className="btn-secondary btn-compact" onClick={() => setEditing(true)}>编辑</button>
        ) : (
          <>
            <button
              className="btn-secondary btn-compact"
              onClick={() => {
                onSave({ description, keywords, category });
                setEditing(false);
              }}
            >
              保存
            </button>
            <button className="btn-secondary btn-compact" onClick={() => setEditing(false)}>取消</button>
          </>
        )}
      </div>

      <div className="file-info-grid">
        <div className="k">文件名</div><div className="v">{details.fileName}</div>
        <div className="k">路径</div><div className="v text-ellipsis">{details.filePath}</div>
        <div className="k">格式</div><div className="v">{details.format || '-'}</div>
        <div className="k">采样率</div><div className="v">{details.sampleRate} Hz</div>
        <div className="k">声道</div><div className="v">{details.channels}</div>
        <div className="k">时长</div><div className="v">{Math.round(details.duration)} s</div>
        <div className="k">文件大小</div><div className="v">{Math.round((details.fileSize || 0) / (1024 * 1024) * 10) / 10} MB</div>
        <div className="k">创建/更新</div><div className="v">{details.fileMtime || '-'}</div>
        <div className="k">入库时间</div><div className="v">{details.dateAdded || '-'}</div>
      </div>

      <div className="file-info-edit">
        <div className="k">描述</div>
        {editing ? (
          <textarea className="glass-input text-area" value={description} onChange={(e) => setDescription(e.target.value)} />
        ) : (
          <div className="v block">{description || '-'}</div>
        )}

        <div className="k">关键词</div>
        {editing ? (
          <input className="glass-input" value={keywords} onChange={(e) => setKeywords(e.target.value)} />
        ) : (
          <div className="v block">{keywords || '-'}</div>
        )}

        <div className="k">分类</div>
        {editing ? (
          <input className="glass-input" value={category} onChange={(e) => setCategory(e.target.value)} />
        ) : (
          <div className="v block">{category || '-'}</div>
        )}
      </div>

    </div>
  );
}
